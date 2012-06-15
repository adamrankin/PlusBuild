#include "vtkBkProFocusVideoSource.h"
#include "vtkVideoBuffer.h"

#include "PlusBkProFocusReceiver.h"

// BK Includes
#include "AcquisitionGrabberSapera.h"
#include "AcquisitionInjector.h"
#include "AcquisitionSettings.h"
#include "CommandAndControl.h"
#include "ParamConnectionSettings.h"
#include "BmodeViewDataReceiver.h"
#include "SaperaViewDataReceiver.h"

vtkCxxRevisionMacro(vtkBkProFocusVideoSource, "$Revision: 1.0$");
vtkStandardNewMacro(vtkBkProFocusVideoSource);

class vtkBkProFocusVideoSource::vtkInternal
{
public:
  vtkBkProFocusVideoSource *External;

  ParamConnectionSettings BKparamSettings; // parConnectSettings, for read/write settings from ini file

  AcquisitionInjector BKAcqInjector; // injector
  AcquisitionSettings BKAcqSettings; // settings
  AcquisitionGrabberSapera BKAcqSapera; // sapera
  BmodeViewDataReceiver BKBModeView; // bmodeView; 
  SaperaViewDataReceiver* pBKSaperaView; // saperaView
  PlusBkProFocusReceiver PlusReceiver;

  CmdCtrlSettings BKcmdCtrlSettings; // cmdCtrlSet
  CommandAndControl* pBKcmdCtrl; // cmdctrl
    
  vtkBkProFocusVideoSource::vtkInternal::vtkInternal(vtkBkProFocusVideoSource* external) 
  {
    this->External = external;
    this->pBKSaperaView=NULL;
    this->pBKcmdCtrl=NULL;
    this->PlusReceiver.SetPlusVideoSource(this->External);
  }

  virtual vtkBkProFocusVideoSource::vtkInternal::~vtkInternal() 
  {    
    this->PlusReceiver.SetPlusVideoSource(NULL);
    delete this->pBKSaperaView;
    this->pBKSaperaView=NULL;
    delete this->pBKcmdCtrl;
    this->pBKcmdCtrl=NULL;
    this->External = NULL;
  }  
};

//----------------------------------------------------------------------------
vtkBkProFocusVideoSource::vtkBkProFocusVideoSource()
{
  this->Internal = new vtkInternal(this);
 
  this->SpawnThreadForRecording=0;
  this->IniFileName=NULL;
  this->ShowSaperaWindow=false;
  this->ShowBModeWindow=false;

  SetLogFunc(LogInfoMessageCallback);
	SetDbgFunc(LogDebugMessageCallback);
}

//----------------------------------------------------------------------------
vtkBkProFocusVideoSource::~vtkBkProFocusVideoSource()
{
  SetIniFileName(NULL);

  delete this->Internal;
  this->Internal=NULL;
}

//----------------------------------------------------------------------------
void vtkBkProFocusVideoSource::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
} 

//----------------------------------------------------------------------------
void vtkBkProFocusVideoSource::LogInfoMessageCallback(char *msg)
{
	LOG_INFO(msg);
}

//----------------------------------------------------------------------------
void vtkBkProFocusVideoSource::LogDebugMessageCallback(char *msg)
{
	LOG_INFO(msg);
}


//----------------------------------------------------------------------------
PlusStatus vtkBkProFocusVideoSource::InternalConnect()
{
  std::string iniFilePath;
  GetFullIniFilePath(iniFilePath);
  if (!this->Internal->BKparamSettings.LoadSettingsFromIniFile(iniFilePath.c_str()))
  {
    LOG_ERROR("Could not lot BK parameter settings from file: "<<iniFilePath.c_str());
    return PLUS_FAIL;
  }

  LOG_DEBUG("BK scanner address: " << this->Internal->BKparamSettings.GetScannerAddress());
	LOG_DEBUG("BK scanner OEM port: " << this->Internal->BKparamSettings.GetOemPort());
	LOG_DEBUG("BK scanner toolbox port: " << this->Internal->BKparamSettings.GetToolboxPort());
  
	this->Internal->BKcmdCtrlSettings.useConsole = true;
	this->Internal->BKcmdCtrlSettings.useConsoleLastOkUseCase = true;

	this->Internal->pBKcmdCtrl = new CommandAndControl(&this->Internal->BKparamSettings, &this->Internal->BKcmdCtrlSettings);
	
  int numSamples = 0;
	int numLines = 0;
  if (!this->Internal->pBKcmdCtrl->CalcSaperaBufSize(&numSamples, &numLines))
	{
    LOG_ERROR("Failed to get Sapera framegrabber buffer size for RF data");
    delete this->Internal->pBKcmdCtrl;
    this->Internal->pBKcmdCtrl=NULL;
    return PLUS_FAIL;
  }

    // Clear buffer on connect because the new frames that we will acquire might have a different size 
  this->GetBuffer()->Clear();  
  
  if (!this->Internal->BKAcqSettings.LoadIni(iniFilePath.c_str()))
  {
    LOG_ERROR("Failed to load acquisition settings from file: "<<iniFilePath.c_str());
    delete this->Internal->pBKcmdCtrl;
    this->Internal->pBKcmdCtrl=NULL;
    return PLUS_FAIL;
  }
		
  this->Internal->BKAcqSettings.SetRFLineLength(numSamples);
  this->Internal->BKAcqSettings.SetLinesPerFrame(numLines);
  this->Internal->BKAcqSettings.SetFramesToGrab(0); // continuous

	if (!this->Internal->BKAcqSapera.Init(this->Internal->BKAcqSettings))
  {
    LOG_ERROR("Failed to initialize framegrabber");
    delete this->Internal->pBKcmdCtrl;
    this->Internal->pBKcmdCtrl=NULL;
    return PLUS_FAIL;
  }

  this->Internal->pBKSaperaView = new SaperaViewDataReceiver(this->Internal->BKAcqSapera.GetBuffer());

  if (this->ShowSaperaWindow)
  {
    // show Sapera viewer
    this->Internal->BKAcqInjector.AddDataReceiver(this->Internal->pBKSaperaView);
  }

  if (this->ShowBModeWindow)
  {
    // show B-mode image  
    this->Internal->BKAcqInjector.AddDataReceiver(&this->Internal->BKBModeView); 
  }
  
  // send frames to this video source
  this->Internal->BKAcqInjector.AddDataReceiver(&this->Internal->PlusReceiver);

  return PLUS_SUCCESS;
}
	
//----------------------------------------------------------------------------
PlusStatus vtkBkProFocusVideoSource::InternalDisconnect()
{
  this->Internal->BKAcqSapera.Destroy();

  this->Internal->BKAcqInjector.RemoveDataReceiver(&this->Internal->PlusReceiver);

  if (this->ShowBModeWindow)
  {
    this->Internal->BKAcqInjector.RemoveDataReceiver(&this->Internal->BKBModeView);
  }

  if (this->ShowSaperaWindow)
  {
    this->Internal->BKAcqInjector.RemoveDataReceiver(this->Internal->pBKSaperaView);
  }
  
  delete this->Internal->pBKSaperaView;
  this->Internal->pBKSaperaView=NULL;
  delete this->Internal->pBKcmdCtrl;
  this->Internal->pBKcmdCtrl=NULL;
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkBkProFocusVideoSource::InternalStartRecording()
{
  if (!this->Internal->BKAcqSapera.StartGrabbing(&this->Internal->BKAcqInjector))
  {
    LOG_ERROR("Failed to start grabbing");
    return PLUS_FAIL;
  }
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkBkProFocusVideoSource::InternalStopRecording()
{
  if (!this->Internal->BKAcqSapera.StopGrabbing())
  {
    LOG_ERROR("Failed to start grabbing");
    return PLUS_FAIL;
  }
  return PLUS_SUCCESS;
}
  
//----------------------------------------------------------------------------
void vtkBkProFocusVideoSource::NewFrameCallback(void* pixelDataPtr, const int frameSizeInPix[2], int numberOfBitsPerPixel)
{      
  // If the buffer is empty, set the pixel type and frame size to the first received properties 
  if ( this->GetBuffer()->GetNumberOfItems() == 0 )
  {
    switch (numberOfBitsPerPixel)
    {
      case 8: this->GetBuffer()->SetPixelType(itk::ImageIOBase::UCHAR); break;
      case 16: this->GetBuffer()->SetPixelType(itk::ImageIOBase::SHORT); break;
      default:
        LOG_ERROR("Unsupported bits per pixel: "<<numberOfBitsPerPixel<<", skip this frame");
        return;
    }      
    this->GetBuffer()->SetFrameSize( frameSizeInPix[0], frameSizeInPix[1] );
  } 
  
  this->Buffer->AddItem(pixelDataPtr, this->GetUsImageOrientation(), frameSizeInPix, itk::ImageIOBase::UCHAR, 0, this->FrameNumber);
  this->FrameNumber++;
}


//-----------------------------------------------------------------------------
PlusStatus vtkBkProFocusVideoSource::ReadConfiguration(vtkXMLDataElement* config)
{
  LOG_TRACE("vtkBkProFocusVideoSource::ReadConfiguration"); 
  if ( config == NULL )
  {
    LOG_ERROR("Unable to configure BK ProFocus video source! (XML data element is NULL)"); 
    return PLUS_FAIL; 
  }

  Superclass::ReadConfiguration(config); 

  vtkXMLDataElement* dataCollectionConfig = config->FindNestedElementWithName("DataCollection");
  if (dataCollectionConfig == NULL)
  {
    LOG_ERROR("Cannot find DataCollection element in XML tree!");
    return PLUS_FAIL;
  }

  vtkXMLDataElement* imageAcquisitionConfig = dataCollectionConfig->FindNestedElementWithName("ImageAcquisition"); 
  if (imageAcquisitionConfig == NULL) 
  {
    LOG_ERROR("Unable to find ImageAcquisition element in configuration XML structure!");
    return PLUS_FAIL;
  }

  const char* iniFileName = imageAcquisitionConfig->GetAttribute("IniFileName"); 
  if ( iniFileName != NULL) 
  {
    this->SetIniFileName(iniFileName); 
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
PlusStatus vtkBkProFocusVideoSource::WriteConfiguration(vtkXMLDataElement* config)
{
  // Write superclass configuration
  Superclass::WriteConfiguration(config); 

  if ( config == NULL )
  {
    LOG_ERROR("Config is invalid");
    return PLUS_FAIL;
  }

  vtkXMLDataElement* dataCollectionConfig = config->FindNestedElementWithName("DataCollection");
  if (dataCollectionConfig == NULL)
  {
    LOG_ERROR("Cannot find DataCollection element in XML tree!");
    return PLUS_FAIL;
  }

  vtkXMLDataElement* imageAcquisitionConfig = dataCollectionConfig->FindNestedElementWithName("ImageAcquisition"); 
  if (imageAcquisitionConfig == NULL) 
  {
    LOG_ERROR("Cannot find ImageAcquisition element in XML tree!");
    return PLUS_FAIL;
  }

  imageAcquisitionConfig->SetAttribute("IniFileName", this->IniFileName);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
PlusStatus vtkBkProFocusVideoSource::GetFullIniFilePath(std::string &fullPath)
{
  if (this->IniFileName==NULL)
  {
    LOG_ERROR("Ini file name has not been set");
    return PLUS_FAIL;
  }
  fullPath=vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationDirectory() + std::string("/") + this->IniFileName;
  return PLUS_SUCCESS;
}
