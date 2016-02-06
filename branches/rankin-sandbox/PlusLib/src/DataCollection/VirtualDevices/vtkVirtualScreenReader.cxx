/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#include "PlusConfigure.h"
#include "vtkObjectFactory.h"
#include "vtkPlusChannel.h"
#include "vtkDataCollector.h"
#include "vtkPlusDataSource.h"
#include "vtkTrackedFrameList.h"
#include "vtkVirtualScreenReader.h"

#include <tesseract/baseapi.h>
#include <tesseract/strngs.h>
#include <allheaders.h>

//----------------------------------------------------------------------------

vtkStandardNewMacro(vtkVirtualScreenReader);

//----------------------------------------------------------------------------

namespace
{
  const double SAMPLING_SKIPPING_MARGIN_SEC = 0.1;
  const double DELAY_ON_SENDING_ERROR_SEC = 0.02;
  const char* PARAMETER_LIST_TAG_NAME = "ScreenFields";
  const char* PARAMETER_TAG_NAME = "Field";
  const char* PARAMETER_NAME_ATTRIBUTE = "Name";
  const char* PARAMETER_CHANNEL_ATTRIBUTE = "Channel";
  const char* PARAMETER_ORIGIN_ATTRIBUTE = "ScreenRegionOrigin";
  const char* PARAMETER_SIZE_ATTRIBUTE = "ScreenRegionSize";
  const int PARAMETER_DEPTH_BITS = 8;
  const char* DEFAULT_LANGUAGE = "eng";
}

//----------------------------------------------------------------------------
vtkVirtualScreenReader::vtkVirtualScreenReader()
  : vtkPlusDevice()
  , Language(NULL)
  , TrackedFrames(vtkTrackedFrameList::New())
{
  // The data capture thread will be used to regularly check the input devices and generate and update the output
  this->StartThreadForInternalUpdates = true;
  this->AcquisitionRate = vtkPlusDevice::VIRTUAL_DEVICE_FRAME_RATE;
}

//----------------------------------------------------------------------------
vtkVirtualScreenReader::~vtkVirtualScreenReader()
{
  TrackedFrames->Delete();
  TrackedFrames = NULL;
}

//----------------------------------------------------------------------------
void vtkVirtualScreenReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualScreenReader::InternalUpdate()
{
  std::map<double, int> queriedFramesIndexes;
  std::vector<vtkSmartPointer<TrackedFrame> > queriedFrames;

  for( ChannelFieldListMapIterator it = this->RecognitionFields.begin(); it != this->RecognitionFields.end(); ++it )
  {
    vtkPlusChannel* channel = it->first;

    for( FieldListIterator fieldIt = it->second.begin(); fieldIt != it->second.end(); ++fieldIt )
    {
      ScreenFieldParameter* parameter = *fieldIt;
      vtkSmartPointer<TrackedFrame> frame;

      // Attempt to find the frame already retrieved
      frame = FindOrQueryFrame(queriedFramesIndexes, parameter, queriedFrames);

      if( frame == NULL )
      {
        LOG_ERROR("Unable to find or query a frame for parameter: " << parameter->ParameterName << ". Skipping.");
        continue;
      }

      // We have a frame, let's parse it      
      vtkImageDataToPix(frame, parameter);

      
      this->TesseractAPI->SetImage(parameter->ReceivedFrame);
      char* text_out = this->TesseractAPI->GetUTF8Text();
      parameter->LatestParameterValue = std::string(text_out);
      delete [] text_out;
    }
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
int vtkVirtualScreenReader::vtkImageDataToPix(TrackedFrame* frame, ScreenFieldParameter* parameter)
{
  PlusVideoFrame::GetOrientedClippedImage(frame->GetImageData()->GetImage(), PlusVideoFrame::FlipInfoType(), 
    frame->GetImageData()->GetImageType(), parameter->ScreenRegion, parameter->Origin, parameter->Size);

  unsigned int *data = pixGetData(parameter->ReceivedFrame);
  int wpl = pixGetWpl(parameter->ReceivedFrame);
  int bpl = ( (8*parameter->Size[0]) + 7) / 8;
  unsigned int *line;
  unsigned char val8;

  static int someVal = 0;
  int coords[3] = {0,0,0};
  for(int y = 0; y < parameter->Size[1]; y++)
  {
    coords[1] = parameter->Size[1]-y-1;
    line = data + y * wpl;
    for(int x = 0; x < bpl; x++)
    {
      coords[0] = x;
      val8 = (*(unsigned char*)parameter->ScreenRegion->GetScalarPointer(coords));
      SET_DATA_BYTE(line,x,val8);
    }
  }
  return bpl;
}

//----------------------------------------------------------------------------
vtkSmartPointer<TrackedFrame> vtkVirtualScreenReader::FindOrQueryFrame(std::map<double, int>& QueriedFramesIndexes, ScreenFieldParameter* parameter, std::vector<vtkSmartPointer<TrackedFrame> >& QueriedFrames)
{
  double mostRecent(-1);
  if( parameter->SourceChannel->GetMostRecentTimestamp(mostRecent) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to retrieve most recent timestamp for parameter " << parameter->ParameterName);
    return NULL;
  }

  std::map<double, int>::iterator frameIt = QueriedFramesIndexes.find(mostRecent);
  if( frameIt == QueriedFramesIndexes.end() )
  {
    this->TrackedFrames->Clear();
    double aTimestamp(UNDEFINED_TIMESTAMP);
    if ( parameter->SourceChannel->GetTrackedFrameList(aTimestamp, this->TrackedFrames, 1) != PLUS_SUCCESS )
    {
      LOG_INFO("Failed to get tracked frame list from data collector."); 
      return NULL;
    }
    double timestamp = TrackedFrames->GetTrackedFrame(0)->GetTimestamp();

    // Copy the frame so it isn't lost when the tracked frame list is cleared
    vtkSmartPointer<TrackedFrame> frame = vtkSmartPointer<TrackedFrame>::New();
    *frame = (*TrackedFrames->GetTrackedFrame(0));

    // Record the index of this timestamp
    QueriedFramesIndexes[timestamp] = QueriedFrames.size();
    QueriedFrames.push_back(frame);

    return frame;
  }
  else
  {
    return QueriedFrames[frameIt->second];
  }
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualScreenReader::InternalConnect()
{
  this->TesseractAPI = new tesseract::TessBaseAPI();
  this->TesseractAPI->Init(NULL, Language, tesseract::OEM_DEFAULT);
  this->TesseractAPI->SetPageSegMode(tesseract::PSM_AUTO);

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualScreenReader::InternalDisconnect()
{
  delete this->TesseractAPI;
  this->TesseractAPI = NULL;

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualScreenReader::ReadConfiguration( vtkXMLDataElement* rootConfigElement)
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_READING(deviceConfig, rootConfigElement);

  this->SetLanguage(DEFAULT_LANGUAGE);
  XML_READ_STRING_ATTRIBUTE_OPTIONAL(Language, deviceConfig);

  XML_FIND_NESTED_ELEMENT_OPTIONAL(ScreenFields, deviceConfig, PARAMETER_LIST_TAG_NAME);

  for( int i = 0; i < ScreenFields->GetNumberOfNestedElements(); ++i )
  {
    vtkXMLDataElement* fieldElement = ScreenFields->GetNestedElement(i);

    if( STRCASECMP(fieldElement->GetName(), PARAMETER_TAG_NAME) != 0 )
    {
      continue;
    }

    const char* channelName = fieldElement->GetAttribute(PARAMETER_CHANNEL_ATTRIBUTE);
    vtkPlusChannel* aChannel;
    if( channelName == NULL || this->GetDataCollector()->GetChannel(aChannel, channelName) != PLUS_SUCCESS)
    {
      LOG_ERROR("Cannot build field scanner. Input " << PARAMETER_CHANNEL_ATTRIBUTE << " is not defined or invalid " << PARAMETER_CHANNEL_ATTRIBUTE << " name specified.");
      continue;
    }

    if( fieldElement->GetAttribute(PARAMETER_NAME_ATTRIBUTE) == NULL )
    {
      LOG_ERROR("Parameter " << PARAMETER_NAME_ATTRIBUTE << " not defined. Unable to build field scanner.");
      continue;
    }

    int origin[2] = {-1,-1};
    int size[2] = {-1,-1};
    fieldElement->GetVectorAttribute(PARAMETER_ORIGIN_ATTRIBUTE, 2, origin);
    fieldElement->GetVectorAttribute(PARAMETER_SIZE_ATTRIBUTE, 2, size);
    if( origin[0] < 0 || origin[1] < 0 || size[0] < 0 || size[1] < 0 )
    {
      LOG_ERROR("Invalid definition for " << PARAMETER_ORIGIN_ATTRIBUTE << " and " << PARAMETER_SIZE_ATTRIBUTE << ". Unable to build field scanner.");
      continue;
    }

    ScreenFieldParameter* parameter = new ScreenFieldParameter();
    parameter->ParameterName = std::string(fieldElement->GetAttribute(PARAMETER_NAME_ATTRIBUTE));
    parameter->SourceChannel = aChannel;
    parameter->Origin[0] = origin[0];
    parameter->Origin[1] = origin[1];
    parameter->Size[0] = size[0];
    parameter->Size[1] = size[1];
    parameter->ReceivedFrame = pixCreate(parameter->Size[0], parameter->Size[1], PARAMETER_DEPTH_BITS);
    parameter->ScreenRegion = vtkSmartPointer<vtkImageData>::New();
    parameter->ScreenRegion->SetExtent(0, size[0]-1, 0, size[1]-1, 0, 0);
    parameter->ScreenRegion->AllocateScalars(VTK_UNSIGNED_CHAR, 1); // Black and white images for now
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualScreenReader::WriteConfiguration(vtkXMLDataElement* rootConfigElement)
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_WRITING(deviceConfig, rootConfigElement);

  if( STRCASECMP(this->Language, DEFAULT_LANGUAGE) != 0 )
  {
    XML_WRITE_STRING_ATTRIBUTE_IF_NOT_NULL(Language, deviceConfig);
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVirtualScreenReader::NotifyConfigured()
{
  if( this->InputChannels.size() < 1 )
  {
    LOG_ERROR("Screen reader needs at least one input image to analyze. Please add an input channel with video data.");
    return PLUS_FAIL;
  }

  if( this->RecognitionFields.size() < 1 )
  {
    LOG_ERROR("Screen reader has no fields defined. There's nothing for me to do!");
  }

  return PLUS_SUCCESS;
}