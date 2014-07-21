/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#include "PlusConfigure.h"
#include "TrackedFrame.h"
#include "vtkObjectFactory.h"
#include "vtkPlusBuffer.h"
#include "vtkPlusDataSource.h"
#include "vtkVirtualDiscCapture.h"
#include "vtksys/SystemTools.hxx"

//----------------------------------------------------------------------------

vtkCxxRevisionMacro(vtkVirtualDiscCapture, "$Revision: 1.0$");
vtkStandardNewMacro(vtkVirtualDiscCapture);

static const int MAX_ALLOWED_RECORDING_LAG_SEC = 3.0; // if the recording lags more than this then it'll skip frames to catch up
static const int DISABLE_FRAME_BUFFER = -1;

//----------------------------------------------------------------------------
vtkVirtualDiscCapture::vtkVirtualDiscCapture()
: vtkPlusDevice()
, m_RecordedFrames(vtkTrackedFrameList::New())
, m_LastAlreadyRecordedFrameTimestamp(UNDEFINED_TIMESTAMP)
, m_NextFrameToBeRecordedTimestamp(0.0)
, m_SamplingFrameRate(8)
, RequestedFrameRate(0.0)
, ActualFrameRate(0.0)
, m_FirstFrameIndexInThisSegment(0)
, m_TimeWaited(0.0)
, m_LastUpdateTime(0.0)
, BaseFilename("TrackedImageSequence.mha")
, m_Writer(vtkMetaImageSequenceIO::New())
, EnableFileCompression(false)
, m_HeaderPrepared(false)
, TotalFramesRecorded(0)
, EnableCapturing(false)
, FrameBufferSize(DISABLE_FRAME_BUFFER)
, WriterAccessMutex(vtkSmartPointer<vtkRecursiveCriticalSection>::New())
, GracePeriodLogLevel(vtkPlusLogger::LOG_LEVEL_DEBUG)
{
  this->MissingInputGracePeriodSec=2.0;
  m_RecordedFrames->SetValidationRequirements(REQUIRE_UNIQUE_TIMESTAMP); 

  // The data capture thread will be used to regularly read the frames and write to disk
  this->StartThreadForInternalUpdates = true;
}

//----------------------------------------------------------------------------
vtkVirtualDiscCapture::~vtkVirtualDiscCapture()
{
  if( m_HeaderPrepared )
  {
    this->CloseFile();
  }

  if (m_RecordedFrames != NULL) {
    m_RecordedFrames->Delete();
    m_RecordedFrames = NULL;
  }

  if( m_Writer != NULL )
  {
    m_Writer->Delete();
    m_Writer = NULL;
  }
}

//----------------------------------------------------------------------------
void vtkVirtualDiscCapture::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::ReadConfiguration( vtkXMLDataElement* rootConfigElement)
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_READING(deviceConfig, rootConfigElement);

  XML_READ_STRING_ATTRIBUTE_OPTIONAL(BaseFilename, deviceConfig);
  XML_READ_BOOL_ATTRIBUTE_OPTIONAL(EnableFileCompression, deviceConfig);
  XML_READ_BOOL_ATTRIBUTE_OPTIONAL(EnableCapturing, deviceConfig);

  this->SetRequestedFrameRate(15.0); // default
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, RequestedFrameRate, deviceConfig);

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, FrameBufferSize, deviceConfig);

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::WriteConfiguration( vtkXMLDataElement* rootConfig)
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_WRITING(deviceElement, rootConfig);
  deviceElement->SetAttribute("EnableCapture", this->EnableCapturing ? "TRUE" : "FALSE" );
  deviceElement->SetDoubleAttribute("RequestedFrameRate", this->GetRequestedFrameRate() );
  return PLUS_SUCCESS;
}


//----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::InternalConnect()
{
  bool lowestRateKnown=false;
  double lowestRate=30; // just a usual value (FPS)
  for( ChannelContainerConstIterator it = this->InputChannels.begin(); it != this->InputChannels.end(); ++it )
  {
    vtkPlusChannel* anInputStream = (*it);
    if( anInputStream->GetOwnerDevice()->GetAcquisitionRate() < lowestRate || !lowestRateKnown)
    {
      lowestRate = anInputStream->GetOwnerDevice()->GetAcquisitionRate();
      lowestRateKnown=true;
    }
  }
  if (lowestRateKnown)
  {
    this->AcquisitionRate = lowestRate;
  }
  else
  {
    LOG_WARNING("vtkVirtualDiscCapture acquisition rate is not known");
  }

  if ( OpenFile() != PLUS_SUCCESS )
  {
    return PLUS_FAIL;
  }

  m_LastUpdateTime = vtkAccurateTimer::GetSystemTime();

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::InternalDisconnect()
{ 
  this->EnableCapturing = false;

  // If outstanding frames to be written, deal with them
  if( m_RecordedFrames->GetNumberOfTrackedFrames() != 0 && m_HeaderPrepared )
  {
    if( m_Writer->AppendImagesToHeader() != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to append image data to header.");
      this->Disconnect();
      return PLUS_FAIL;
    }
    if( m_Writer->AppendImages() != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to append images. Stopping recording at timestamp: " << m_LastAlreadyRecordedFrameTimestamp );
      this->Disconnect();
      return PLUS_FAIL;
    }

    this->ClearRecordedFrames();
  }
  PlusStatus status = this->CloseFile();
  return status;
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::OpenFile(const char* aFilename)
{
  PlusLockGuard<vtkRecursiveCriticalSection> writerLock(this->WriterAccessMutex);

  // Because this virtual device continually appends data to the file, we cannot do live compression
  m_Writer->SetUseCompression(false);
  m_Writer->SetTrackedFrameList(m_RecordedFrames);

  if( aFilename == NULL || strlen(aFilename) == 0 )
  {
    std::string filenameRoot = vtksys::SystemTools::GetFilenameWithoutExtension(BaseFilename);
    std::string ext = vtksys::SystemTools::GetFilenameExtension(BaseFilename);
    if( ext.empty() )
    {
      ext = ".mha";
    }
    m_CurrentFilename = filenameRoot + "_" + vtksys::SystemTools::GetCurrentDateTime("%Y%m%d_%H%M%S") + ext;
    aFilename = m_CurrentFilename.c_str();
  }
  else
  {
    m_CurrentFilename = aFilename;
  }

  // Need to set the filename before finalizing header, because the pixel data file name depends on the file extension
  m_Writer->SetFileName(aFilename);

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::CloseFile(const char* aFilename)
{
  // Fix the header to write the correct number of frames
  PlusLockGuard<vtkRecursiveCriticalSection> writerLock(this->WriterAccessMutex);

  if (!m_HeaderPrepared)
  {
    // nothing has been prepared, so nothing to finalize
    return PLUS_SUCCESS;
  }

  if( aFilename != NULL && strlen(aFilename) != 0 )
  {
    // Need to set the filename before finalizing header, because the pixel data file name depends on the file extension
    m_Writer->SetFileName(aFilename);
    m_CurrentFilename = aFilename;
  }

  // Do we have any outstanding unwritten data?
  if( m_RecordedFrames->GetNumberOfTrackedFrames() != 0 )
  {
    this->WriteFrames(true);
  }

  std::ostringstream dimSizeStr; 
  int dimensions[3]={0};
  dimensions[0] = m_Writer->GetDimensions()[0];
  dimensions[1] = m_Writer->GetDimensions()[1];
  dimensions[2] = this->TotalFramesRecorded;
  dimSizeStr << dimensions[0] << " " << dimensions[1] << " " << dimensions[2];
  m_Writer->GetTrackedFrameList()->SetCustomString("DimSize", dimSizeStr.str().c_str());
  m_Writer->UpdateFieldInImageHeader("DimSize");
  m_Writer->FinalizeHeader();

  m_Writer->Close();

  std::string fullPath = vtkPlusConfig::GetInstance()->GetOutputPath(m_CurrentFilename);
  std::string path = vtksys::SystemTools::GetFilenamePath(fullPath); 
  std::string filename = vtksys::SystemTools::GetFilenameWithoutExtension(fullPath); 
  std::string configFileName = path + "/" + filename + "_config.xml";
  PlusCommon::PrintXML(configFileName.c_str(), vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData());

  if( EnableFileCompression )
  {
    if( this->CompressFile() != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to compress file.");
      return PLUS_FAIL;
    }
  }

  m_HeaderPrepared = false;
  this->TotalFramesRecorded = 0;
  m_RecordedFrames->Clear();

  if ( OpenFile() != PLUS_SUCCESS )
  {
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------

PlusStatus vtkVirtualDiscCapture::InternalUpdate()
{
  if (!this->EnableCapturing)
  {
    // Capturing is disabled
    return PLUS_SUCCESS;
  }

  if( m_LastUpdateTime == 0.0 )
  {
    m_LastUpdateTime = vtkAccurateTimer::GetSystemTime();
  }
  if( m_NextFrameToBeRecordedTimestamp == 0.0 )
  {
    m_NextFrameToBeRecordedTimestamp = vtkAccurateTimer::GetSystemTime();
  }
  double startTimeSec = vtkAccurateTimer::GetSystemTime();

  m_TimeWaited += startTimeSec - m_LastUpdateTime;

  if( m_TimeWaited < GetSamplingPeriodSec() )
  {
    // Nothing to do yet
    return PLUS_SUCCESS;
  }

  m_TimeWaited = 0.0;

  double maxProcessingTimeSec = GetSamplingPeriodSec() * 2.0; // put a hard limit on the max processing time to make sure the application remains responsive during recording
  double requestedFramePeriodSec = 0.1;
  if (this->RequestedFrameRate > 0)
  {
    requestedFramePeriodSec = 1.0 / this->RequestedFrameRate;
  }
  else
  {
    LOG_WARNING("RequestedFrameRate is invalid");
  }

  if( this->HasGracePeriodExpired() )
  {
    this->GracePeriodLogLevel = vtkPlusLogger::LOG_LEVEL_WARNING;
  }
  
  PlusLockGuard<vtkRecursiveCriticalSection> writerLock(this->WriterAccessMutex);
  if (!this->EnableCapturing)
  {
    // While this thread was waiting for the unlock, capturing was disabled, so cancel the update now
    return PLUS_SUCCESS;
  }

  int nbFramesBefore = m_RecordedFrames->GetNumberOfTrackedFrames();
  if ( this->GetInputTrackedFrameListSampled(m_LastAlreadyRecordedFrameTimestamp, m_NextFrameToBeRecordedTimestamp, m_RecordedFrames, requestedFramePeriodSec, maxProcessingTimeSec) != PLUS_SUCCESS )
  {
    LOG_ERROR("Error while getting tracked frame list from data collector during capturing. Last recorded timestamp: " << std::fixed << m_NextFrameToBeRecordedTimestamp ); 
  }  
  int nbFramesAfter = m_RecordedFrames->GetNumberOfTrackedFrames();

  if( this->WriteFrames() != PLUS_SUCCESS )
  {
    LOG_ERROR(this->GetDeviceId() << ": Unable to write " << nbFramesAfter - nbFramesBefore << " frames.");
    return PLUS_FAIL;
  }

  this->TotalFramesRecorded += nbFramesAfter - nbFramesBefore;

  if( this->TotalFramesRecorded == 0)
  {
    // We haven't received any data so far
    LOG_DYNAMIC("No input data available to capture thread. Waiting until input data arrives.", this->GracePeriodLogLevel);
  }

  // Check whether the recording needed more time than the sampling interval
  double recordingTimeSec = vtkAccurateTimer::GetSystemTime() - startTimeSec;
  if (recordingTimeSec > GetSamplingPeriodSec())
  {
    LOG_WARNING("Recording of frames takes too long time (" << recordingTimeSec << "sec instead of the allocated " << GetSamplingPeriodSec() << "sec). This can cause slow-down of the application and non-uniform sampling. Reduce the acquisition rate or sampling rate to resolve the problem.");
  }

  double recordingLagSec = vtkAccurateTimer::GetSystemTime() - m_NextFrameToBeRecordedTimestamp;
  if (recordingLagSec > MAX_ALLOWED_RECORDING_LAG_SEC)
  {
    LOG_ERROR("Recording cannot keep up with the acquisition. Skip " << recordingLagSec << " seconds of the data stream to catch up.");
    m_NextFrameToBeRecordedTimestamp = vtkAccurateTimer::GetSystemTime();
  }

  m_LastUpdateTime = vtkAccurateTimer::GetSystemTime();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus vtkVirtualDiscCapture::CompressFile()
{
  vtkSmartPointer<vtkMetaImageSequenceIO> reader = vtkSmartPointer<vtkMetaImageSequenceIO>::New();
  std::string fullPath=vtkPlusConfig::GetInstance()->GetOutputPath(BaseFilename);
  reader->SetFileName(fullPath.c_str());

  LOG_DEBUG("Read input sequence metafile: " << fullPath ); 

  if (reader->Read() != PLUS_SUCCESS)
  {    
    LOG_ERROR("Couldn't read sequence metafile: " <<  fullPath ); 
    return PLUS_FAIL;
  }  

  // Now write to disc using compression
  reader->SetUseCompression(true);
  reader->SetFileName(fullPath.c_str());

  if (reader->Write() != PLUS_SUCCESS)
  {    
    LOG_ERROR("Couldn't write sequence metafile: " <<  fullPath ); 
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::NotifyConfigured()
{
  if( !this->OutputChannels.empty() )
  {
    LOG_WARNING("vtkVirtualDiscCapture is expecting no output channel(s) and there are " << this->OutputChannels.size() << " channels. Output channel information will be dropped.");
    this->OutputChannels.clear();
  }

  if( this->InputChannels.empty() )
  {
    LOG_ERROR("No input channel sent to vtkVirtualDiscCapture. Unable to save anything.");
    return PLUS_FAIL;
  }
  vtkPlusChannel* inputChannel=this->InputChannels[0];

  // GetTrackedFrame reads from the OutputChannels
  // For now, place the input stream as an output stream so its data is read
  this->OutputChannels.push_back(inputChannel);
  inputChannel->Register(this); // this device uses this channel, too, se we need to update the reference count to avoid double delete in the destructor

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
bool vtkVirtualDiscCapture::HasUnsavedData() const
{
  return m_HeaderPrepared;
}

//-----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::ClearRecordedFrames()
{
  m_RecordedFrames->Clear();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
void vtkVirtualDiscCapture::InternalWriteOutputChannels( vtkXMLDataElement* rootXMLElement )
{
  // Do not write anything out, disc capture devices don't have output channels in the config
}

//-----------------------------------------------------------------------------
double vtkVirtualDiscCapture::GetMaximumFrameRate()
{
  LOG_TRACE("vtkVirtualDiscCapture::GetMaximumFrameRate");

  return this->GetAcquisitionRate();
}

//-----------------------------------------------------------------------------
double vtkVirtualDiscCapture::GetSamplingPeriodSec()
{
  double samplingPeriodSec = 0.1;
  if (m_SamplingFrameRate > 0)
  {
    samplingPeriodSec = 1.0 / m_SamplingFrameRate;
  }
  else
  {
    LOG_WARNING("m_SamplingFrameRate value is invalid " << m_SamplingFrameRate << ". Use default sampling period of " << samplingPeriodSec << " sec");
  }
  return samplingPeriodSec;
}

//-----------------------------------------------------------------------------
void vtkVirtualDiscCapture::SetEnableCapturing( bool aValue )
{
  this->EnableCapturing = aValue;

  if( aValue )
  {
    m_LastUpdateTime = 0.0;
    m_TimeWaited = 0.0;
    m_LastAlreadyRecordedFrameTimestamp = UNDEFINED_TIMESTAMP;
    m_NextFrameToBeRecordedTimestamp = 0.0;
    m_FirstFrameIndexInThisSegment = 0.0;
    this->RecordingStartTime = vtkAccurateTimer::GetSystemTime(); // reset the starting time for the grace period
  }
}

//-----------------------------------------------------------------------------
void vtkVirtualDiscCapture::SetRequestedFrameRate( double aValue )
{
  LOG_TRACE("vtkVirtualDiscCapture::SetRequestedFrameRate(" << aValue << ")"); 

  double maxFrameRate = this->GetMaximumFrameRate();

  if( aValue > maxFrameRate )
  {
    aValue = maxFrameRate;
  }
  this->RequestedFrameRate = aValue;

  LOG_DEBUG("vtkVirtualDiscCapture requested frame rate changed to " << this->RequestedFrameRate );
}

//-----------------------------------------------------------------------------
double vtkVirtualDiscCapture::GetAcquisitionRate() const
{
  if( this->InputChannels.size() <= 0 )
  {
    return VIRTUAL_DEVICE_FRAME_RATE;
  }
  return this->InputChannels[0]->GetOwnerDevice()->GetAcquisitionRate();
}

//-----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::Reset()
{
  {
    PlusLockGuard<vtkRecursiveCriticalSection> writerLock(this->WriterAccessMutex);

    this->SetEnableCapturing(false);

    if( m_HeaderPrepared )
    {
      // Change the filename to a temporary filename
      std::string tempFilename;
      if( PlusCommon::CreateTemporaryFilename(tempFilename, "") != PLUS_SUCCESS )
      {
        LOG_ERROR("Unable to create temporary file. Check write access.");
      }
      else
      {
        // Risky, file with extension ".mha" might exist... no way to use windows utility to change extension
        // In reality, probably will never be an issue
        std::string mhaFilename(tempFilename);
        mhaFilename.replace(mhaFilename.end()-3, mhaFilename.end(), "mha");
        this->m_Writer->SetFileName(mhaFilename.c_str());

        this->m_Writer->Close();

        vtksys::SystemTools::RemoveFile(tempFilename.c_str());
        vtksys::SystemTools::RemoveFile(mhaFilename.c_str());
      }
    }

    this->ClearRecordedFrames();
    this->m_Writer->GetTrackedFrameList()->Clear();
    m_HeaderPrepared = false;
    TotalFramesRecorded = 0;
  }

  if( this->OpenFile() != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to reset device " << this->GetDeviceId() << ".");
    return PLUS_FAIL;
  }

  m_LastUpdateTime = vtkAccurateTimer::GetSystemTime();

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
bool vtkVirtualDiscCapture::IsFrameBuffered() const
{
  return this->FrameBufferSize != DISABLE_FRAME_BUFFER;
}

//-----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::TakeSnapshot()
{
  if( this->EnableCapturing )
  {
    LOG_ERROR(this->GetDeviceId() << ": Cannot take snapshot while the device is recording.");
    return PLUS_FAIL;
  }

  TrackedFrame trackedFrame;
  if( this->GetInputTrackedFrame(&trackedFrame) != PLUS_SUCCESS )
  {
    LOG_ERROR(this->GetDeviceId() << ": Failed to get tracked frame for the snapshot!");
    return PLUS_FAIL;
  }

  // Check if there are any valid transforms
  std::vector<PlusTransformName> transformNames;
  trackedFrame.GetCustomFrameTransformNameList(transformNames);
  bool validFrame = false;

  if (transformNames.size() == 0)
  {
    validFrame = true;
  }
  else
  {
    for (std::vector<PlusTransformName>::iterator it = transformNames.begin(); it != transformNames.end(); ++it)
    {
      TrackedFrameFieldStatus status = FIELD_INVALID;
      trackedFrame.GetCustomFrameTransformStatus(*it, status);

      if ( status == FIELD_OK )
      {
        validFrame = true;
        break;
      }
    }
  }

  if ( !validFrame )
  {
    LOG_WARNING(this->GetDeviceId() << ": Unable to record tracked frame: All the tool transforms are invalid!"); 
    return PLUS_FAIL;
  }

  // Add tracked frame to the list
  if (m_RecordedFrames->AddTrackedFrame(&trackedFrame, vtkTrackedFrameList::SKIP_INVALID_FRAME) != PLUS_SUCCESS)
  {
    LOG_WARNING(this->GetDeviceId() << ": Frame could not be added because validation failed!");
    return PLUS_FAIL;
  }
  
  if( this->WriteFrames() != PLUS_SUCCESS )
  {
    LOG_ERROR(this->GetDeviceId() << ": Unable to write frames while taking a snapshot.");
    return PLUS_FAIL;
  }

  this->TotalFramesRecorded += 1;

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::WriteFrames(bool force)
{
  if( !m_HeaderPrepared && m_RecordedFrames->GetNumberOfTrackedFrames() != 0 )
  {
    if( m_Writer->PrepareHeader() != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to prepare header.");
      this->Disconnect();
      return PLUS_FAIL;
    }
    m_HeaderPrepared = true;
  }

  // Compute the average frame rate from the ratio of recently acquired frames
  int frame1Index = m_RecordedFrames->GetNumberOfTrackedFrames() - 1; // index of the latest frame
  int frame2Index = frame1Index - this->RequestedFrameRate * 5.0 - 1; // index of an earlier acquired frame (go back by approximately 5 seconds + one frame)
  if (frame2Index < m_FirstFrameIndexInThisSegment)
  {
    // make sure we stay in the current recording segment
    frame2Index = m_FirstFrameIndexInThisSegment;
  }
  if (frame1Index > frame2Index)
  {   
    TrackedFrame* frame1 = m_RecordedFrames->GetTrackedFrame(frame1Index);
    TrackedFrame* frame2 = m_RecordedFrames->GetTrackedFrame(frame2Index);
    if (frame1 != NULL && frame2 != NULL)
    {
      double frameTimeDiff = frame1->GetTimestamp() - frame2->GetTimestamp();
      if (frameTimeDiff > 0)
      {
        this->ActualFrameRate = (frame1Index - frame2Index) / frameTimeDiff;
      }
      else
      {
        this->ActualFrameRate = 0;
      }
    }    
  }

  if( m_RecordedFrames->GetNumberOfTrackedFrames() == 0 )
  {
    return PLUS_SUCCESS;
  }

  if( force || !this->IsFrameBuffered() || 
    ( this->IsFrameBuffered() && m_RecordedFrames->GetNumberOfTrackedFrames() > this->GetFrameBufferSize() ) )
  {
    if( m_Writer->AppendImagesToHeader() != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to append image data to header.");
      this->Disconnect();
      return PLUS_FAIL;
    }
    if( m_Writer->AppendImages() != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to append images. Stopping recording at timestamp: " << m_LastAlreadyRecordedFrameTimestamp );
      this->Disconnect();
      return PLUS_FAIL;
    }

    this->ClearRecordedFrames();
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
int vtkVirtualDiscCapture::OutputChannelCount() const
{
  // Even though we fake one output channel for easy GetTrackedFrame ability, 
  //  we shouldn't return actual output channel size
  return 0;
}

//-----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::GetInputTrackedFrame( TrackedFrame* aFrame )
{
  if( this->OutputChannels.empty() )
  {
    LOG_ERROR("No output channels defined" );
    return PLUS_FAIL;
  }

  return this->OutputChannels[0]->GetTrackedFrame(aFrame);
}

//-----------------------------------------------------------------------------
PlusStatus vtkVirtualDiscCapture::GetInputTrackedFrameListSampled( double &lastAlreadyRecordedFrameTimestamp, double &nextFrameToBeRecordedTimestamp, vtkTrackedFrameList* recordedFrames, double requestedFramePeriodSec, double maxProcessingTimeSec )
{
  if( this->OutputChannels.empty() )
  {
    LOG_ERROR("No output channels defined" );
    return PLUS_FAIL;
  }

  return this->OutputChannels[0]->GetTrackedFrameListSampled(lastAlreadyRecordedFrameTimestamp, nextFrameToBeRecordedTimestamp, recordedFrames, requestedFramePeriodSec, maxProcessingTimeSec);
}