/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkIntersonVideoSource_h
#define __vtkIntersonVideoSource_h

#include "vtkDataCollectionExport.h"

#include "vtkPlusDevice.h"
#include "vtkUSImagingParameters.h"

/*!
  \class vtkIntersonVideoSource 
  \brief Class for acquiring ultrasound images from Interson USB ultrasound systems

  Requires the PLUS_USE_INTERSON option in CMake.
  Requires Interson iSDK2012 (SDK provided by Interson).

  \ingroup PlusLibDataCollection
*/
class vtkDataCollectionExport vtkIntersonVideoSource : public vtkPlusDevice
{
public:
  static vtkIntersonVideoSource *New();
  vtkTypeMacro(vtkIntersonVideoSource,vtkPlusDevice);
  void PrintSelf(ostream& os, vtkIndent indent);   

  virtual bool IsTracker() const { return false; }

  /*! Read configuration from xml data */  
  virtual PlusStatus ReadConfiguration(vtkXMLDataElement* config); 
  /*! Write configuration to xml data */
  virtual PlusStatus WriteConfiguration(vtkXMLDataElement* config);    

  /*! Verify the device is correctly configured */
  virtual PlusStatus NotifyConfigured();

  virtual std::string GetSdkVersion();

  void SetSectorPercent(double value) { this->ImagingParameters->SetSectorPercent(value); }
  void SetIntensity(int value) { this->ImagingParameters->SetIntensity(value); }
  void SetContrast(int value) { this->ImagingParameters->SetContrast(value); }
  void SetDynRangeDb(double value) { this->ImagingParameters->SetDynRangeDb(value); }
  void SetSoundVelocity(double value) { this->ImagingParameters->SetSoundVelocity(value); }

  vtkGetMacro(EnableProbeButtonMonitoring, bool);
  vtkSetMacro(EnableProbeButtonMonitoring, bool);

protected:
  /*! Constructor */
  vtkIntersonVideoSource();
  /*! Destructor */
  ~vtkIntersonVideoSource();

  /*! Device-specific connect */
  virtual PlusStatus InternalConnect();

  /*! Device-specific disconnect */
  virtual PlusStatus InternalDisconnect();

  /*! Device-specific recording start */
  virtual PlusStatus InternalStartRecording();

  /*! Device-specific recording stop */
  virtual PlusStatus InternalStopRecording();

  /*! The internal function which actually does the grab.  */
  PlusStatus InternalUpdate();

  PlusStatus GetFullIniFilePath(std::string &fullPath);

  PlusStatus FreezeDevice(bool freeze);

  PlusStatus WaitForFrame();

  PlusStatus SetDisplayZoomDevice(double zoom);

  PlusStatus GetSampleFrequencyDevice(double& aFreq);

  /* Set the desired probe frequency in Hz. The resulting probe speed will be approximately the value specified */
  PlusStatus SetProbeFrequencyDevice(double aFreq);

  PlusStatus GetProbeVelocityDevice(double& aVel);

  /* Represents the depth, in pixels, the display window will be. This defaults to 512 pixels for newly initialized probes.*/
  PlusStatus SetWindowDepthDevice(int height);

  /* Set the probe depth in mm */
  PlusStatus SetDepthMm(double depthMm);

  /* Set the probe depth in mm */
  PlusStatus SetDepthMmDevice(double depthMm);

  /* Set the image size */
  PlusStatus SetImageSize(int imageSize[2]);

  /* Set the frquency in Mhz */
  PlusStatus SetFrequencyMhz(double freq);

  /* Set the gain in percent */
  PlusStatus SetGainPercent(double gainPercent[3]);
  /* Set the gain in percent in the device */
  PlusStatus SetGainPercentDevice(double gainPercent[3]);

  /* Set the zom factor. */
  PlusStatus SetZoomFactor(double gainPercent);

  /* Each probe has a defined set of allowed modes. 
  These modes are combinations of pulse frequency and sample rate that yield acceptable results
  with that particular probe. While there is no enforcement to use only these modes, one should 
  understand the implications on image quality if one of the allowed modes is not selected.*/
  PlusStatus GetProbeAllowedModes(std::vector< std::pair<double,double> > &allowedModes);

  /*! Get probe name from the device */
  PlusStatus GetProbeNameDevice(std::string& probeName);

  // For internal storage of additional variables (to minimize the number of included headers)
  class vtkInternal;
  vtkInternal* Internal;

  vtkUsImagingParameters* ImagingParameters;

  bool Interpolate;
  bool BidirectionalScan;
  bool Frozen;

  int ClockDivider;
  double ClockFrequencyMHz;
  double SoundVelocity;
  int PulseFrequencyDivider;

  int Brightness;
  int Contrast;
  double LutCenter;
  double LutWindow;
  int ImageSize[2];
  double PulseVoltage;

  double InitialGain;
  double MidGain;
  double FarGain;

  // ProbeButtonPressCount is incremented each time the button on the probe is pressed
  // The value is available in the output channel in the translation component of the ProbeButtonToDummyTransform
  int ProbeButtonPressCount;

  bool EnableProbeButtonMonitoring;

private:
  vtkIntersonVideoSource(const vtkIntersonVideoSource&);  // Not implemented.
  void operator=(const vtkIntersonVideoSource&);  // Not implemented.
};

#endif