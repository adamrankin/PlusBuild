/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

/*!
  \file vtkPhantomRegistrationAutoDetectPivotingTest.cxx 
  \brief This test runs a phantom registration on a recorded data set and 
  compares the results to a baseline
*/ 

#include "PlusConfigure.h"
#include "PlusMath.h"
#include "TrackedFrame.h"
#include "vtkDataCollector.h"
#include "vtkFakeTracker.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkPhantomLandmarkRegistrationAlgo.h"
#include "vtkPlusChannel.h"
#include "vtkPlusConfig.h"
#include "vtkSmartPointer.h"
#include "vtkTrackedFrameList.h"
#include "vtkTransform.h"
#include "vtkTransformRepository.h"
#include "vtkXMLDataElement.h"
#include "vtkXMLUtilities.h"
#include "vtksys/CommandLineArguments.hxx" 
#include "vtksys/SystemTools.hxx"
#include <iostream>
#include <stdlib.h>

#include "vtkAxis.h"
#include "vtkChartXY.h"
#include "vtkContextScene.h"
#include "vtkContextView.h"
#include "vtkWindowToImageFilter.h"
#include "vtkRenderer.h"
#include "vtkPNGWriter.h"
#include "vtkPlot.h"
#include "vtkRenderWindow.h"
#include "vtkReadTrackedSignals.h"

#include "vtkPivotDetectionAlgo.h"

///////////////////////////////////////////////////////////////////
const double ERROR_THRESHOLD_MM = 0.001; // error threshold
const double NUMBER_PIVOTS =8;

PlusStatus CompareRegistrationResultsWithBaseline(const char* baselineFileName, const char* currentResultFileName, const char* phantomCoordinateFrame, const char* referenceCoordinateFrame);

//-----------------------------------------------------------------------------
PlusStatus ConstructTableSignal(std::deque<double> &x, std::deque<double> &y, vtkTable* table,
                                                            double timeCorrection)
{
  // Clear table
  while (table->GetNumberOfColumns() > 0)
  {
    table->RemoveColumn(0);
  }

  //  Create array corresponding to the time values of the tracker plot
  vtkSmartPointer<vtkDoubleArray> arrX = vtkSmartPointer<vtkDoubleArray>::New();
  table->AddColumn(arrX);

  //  Create array corresponding to the metric values of the tracker plot
  vtkSmartPointer<vtkDoubleArray> arrY = vtkSmartPointer<vtkDoubleArray>::New();
  table->AddColumn(arrY);

  // Set the tracker data
  table->SetNumberOfRows(x.size());
  for (int i = 0; i < x.size(); ++i)
  {
    table->SetValue(i, 0, x.at(i) + timeCorrection );
    table->SetValue(i, 1, y.at(i) );
  }

  return PLUS_SUCCESS;
}
//----------------------------------------------------------------------------
void SaveMetricPlot(const char* filename, vtkTable* StylusRef, vtkTable* StylusTipRef, vtkTable* StylusTipFromPivot, std::string &xAxisLabel,
                    std::string &yAxisLabel)
{
  // Set up the view
  vtkSmartPointer<vtkContextView> view = vtkSmartPointer<vtkContextView>::New();
  view->GetRenderer()->SetBackground(1.0, 1.0, 1.0);
  vtkSmartPointer<vtkChartXY> chart =  vtkSmartPointer<vtkChartXY>::New();
  view->GetScene()->AddItem(chart);

  // Add the two line plots    
  vtkPlot *StylusRefLine = chart->AddPlot(vtkChart::POINTS);
  StylusRefLine->SetInputData_vtk5compatible(StylusRef, 0, 1);
  StylusRefLine->SetColor(0,0,1);
  StylusRefLine->SetWidth(0.3);

  vtkPlot *StylusTipRefLine = chart->AddPlot(vtkChart::POINTS);
  StylusTipRefLine->SetInputData_vtk5compatible(StylusTipRef, 0, 1);
  StylusTipRefLine->SetColor(0,1,0);
  StylusTipRefLine->SetWidth(0.3);

  vtkPlot *StylusTipFromPivotLine = chart->AddPlot(vtkChart::LINE);
  StylusTipFromPivotLine->SetInputData_vtk5compatible(StylusTipFromPivot, 0, 1);
  StylusTipFromPivotLine->SetColor(1,0,0);
  StylusTipFromPivotLine->SetWidth(1.0);

  chart->SetShowLegend(true);
  chart->GetAxis(vtkAxis::LEFT)->SetTitle(yAxisLabel.c_str());
  chart->GetAxis(vtkAxis::BOTTOM)->SetTitle(xAxisLabel.c_str());

  // Render plot and save it to file
  vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
  renderWindow->AddRenderer(view->GetRenderer());
  renderWindow->SetSize(1600,1200);
  renderWindow->OffScreenRenderingOn(); 

  vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
  windowToImageFilter->SetInput(renderWindow);
  windowToImageFilter->Update();

  vtkSmartPointer<vtkPNGWriter> writer = vtkSmartPointer<vtkPNGWriter>::New();
  writer->SetFileName(filename);
  writer->SetInputData_vtk5compatible(windowToImageFilter->GetOutput());
  writer->Write();
}

PlusStatus ConstructSignalPlot(vtkTrackedFrameList* trackedStylusTipFrames, std::string intermediateFileOutputDirectory, vtkXMLDataElement* aConfig)
{
  double signalTimeRangeMin = trackedStylusTipFrames->GetTrackedFrame(0)->GetTimestamp();
  double signalTimeRangeMax = trackedStylusTipFrames->GetTrackedFrame(trackedStylusTipFrames->GetNumberOfTrackedFrames()-1)->GetTimestamp();
  std::deque<double> signalTimestamps;
  std::deque<double> signalValues;

  LOG_INFO("Range ["<< signalTimeRangeMin<<"-"<<signalTimeRangeMax<<"] "<<(signalTimeRangeMax-signalTimeRangeMin) << "[s]");
  double frequency = 1/(trackedStylusTipFrames->GetTrackedFrame(1)->GetTimestamp()-trackedStylusTipFrames->GetTrackedFrame(0)->GetTimestamp());
  LOG_INFO("Frequency one frame = "<< frequency);
  frequency=trackedStylusTipFrames->GetNumberOfTrackedFrames()/(signalTimeRangeMax-signalTimeRangeMin);
  LOG_INFO("Frequency average frame = "<< frequency);

  vtkSmartPointer<vtkReadTrackedSignals> trackerDataMetricExtractor = vtkSmartPointer<vtkReadTrackedSignals>::New();

  trackerDataMetricExtractor->SetTrackerFrames(trackedStylusTipFrames);
  trackerDataMetricExtractor->SetSignalTimeRange(signalTimeRangeMin, signalTimeRangeMax);
  trackerDataMetricExtractor->ReadConfiguration(aConfig);

  if (trackerDataMetricExtractor->Update() != PLUS_SUCCESS)
  {
    LOG_ERROR("Failed to get line positions from video frames");
    return PLUS_FAIL;
  }
  trackerDataMetricExtractor->GetTimestamps(signalTimestamps);
  trackerDataMetricExtractor->GetSignalStylusTipSpeed(signalValues);
  vtkSmartPointer<vtkTable> stylusTipSpeedTable=vtkSmartPointer<vtkTable>::New();
  ConstructTableSignal(signalTimestamps, signalValues, stylusTipSpeedTable, 0); 
  stylusTipSpeedTable->GetColumn(0)->SetName("Time [s]");
  stylusTipSpeedTable->GetColumn(1)->SetName("stylusTipSpeed");

  trackerDataMetricExtractor->GetSignalStylusRef(signalValues);
  vtkSmartPointer<vtkTable> stylusRefTable=vtkSmartPointer<vtkTable>::New();
  ConstructTableSignal(signalTimestamps, signalValues,stylusRefTable, 0); 
  stylusRefTable->GetColumn(0)->SetName("Time [s]");
  stylusRefTable->GetColumn(1)->SetName("stylusRef");

  trackerDataMetricExtractor->GetSignalStylusTipRef(signalValues);
  vtkSmartPointer<vtkTable> stylusTipRefTable=vtkSmartPointer<vtkTable>::New();
  ConstructTableSignal(signalTimestamps, signalValues,stylusTipRefTable, 0); 
  stylusTipRefTable->GetColumn(0)->SetName("Time [s]");
  stylusTipRefTable->GetColumn(1)->SetName("stylusTipRef");

  if(stylusTipSpeedTable->GetNumberOfColumns() != 2)
  {
    LOG_ERROR("Error in constructing the vtk tables that are to hold fixed signal. Table has " << 
      stylusTipSpeedTable->GetNumberOfColumns() << " columns, but should have two columns");
    return PLUS_FAIL;
  }

  std::string filename=intermediateFileOutputDirectory + "\\StylusTracked.png";
  std::string xLabel = "Time [s]";
  std::string yLabel = "Position Metric";
  SaveMetricPlot(filename.c_str(), stylusRefTable, stylusTipRefTable,  stylusTipSpeedTable, xLabel, yLabel);
}

int main (int argc, char* argv[])
{
  std::string inputConfigFileName;
  std::string inputBaselineFileName;
  int verboseLevel=vtkPlusLogger::LOG_LEVEL_UNDEFINED;
  std::string inputTrackedStylusTipSequenceMetafile;
  std::string intermediateFileOutputDirectory;
  std::string styluscalibratedConfigFileName;
  vtksys::CommandLineArguments cmdargs;
  cmdargs.Initialize(argc, argv);
  cmdargs.AddArgument("--config-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputConfigFileName, "Configuration file name");
  cmdargs.AddArgument("--baseline-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputBaselineFileName, "Name of file storing baseline calibration results");
  cmdargs.AddArgument("--verbose", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &verboseLevel, "Verbose level (1=error only, 2=warning, 3=info, 4=debug, 5=trace)");
  cmdargs.AddArgument("--intermediate-file-output-dir", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &intermediateFileOutputDirectory, "Directory into which the intermediate files are written");
  cmdargs.AddArgument("--stylus-calibrated-config-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &styluscalibratedConfigFileName, "Configuration file name");
  cmdargs.AddArgument("--tracker-input-seq-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputTrackedStylusTipSequenceMetafile, "Input tracker sequence metafile name with path");
  if ( !cmdargs.Parse() )
  {
    std::cerr << "Problem parsing arguments" << std::endl;
    std::cout << "Help: " << cmdargs.GetHelp() << std::endl;
    exit(EXIT_FAILURE);
  }
  vtkPlusLogger::Instance()->SetLogLevel(verboseLevel);
  LOG_INFO("Initialize"); 
  // Read PivotDetection configuration
  vtkSmartPointer<vtkXMLDataElement> configPivotDetection = vtkSmartPointer<vtkXMLDataElement>::Take(vtkXMLUtilities::ReadElementFromFile(inputConfigFileName.c_str()));
  if (configPivotDetection == NULL)
  {  
    LOG_ERROR("Unable to read PivotDetection configuration from file " << inputConfigFileName.c_str()); 
    exit(EXIT_FAILURE);
  }
  vtkPlusConfig::GetInstance()->SetDeviceSetConfigurationData(configPivotDetection); 
  if ( intermediateFileOutputDirectory.empty() )
  {
    intermediateFileOutputDirectory = vtkPlusConfig::GetInstance()->GetOutputDirectory();
  }
  // Read StylusCalibration configuration
  vtkSmartPointer<vtkXMLDataElement> configStylusCalibration = vtkSmartPointer<vtkXMLDataElement>::Take(vtkXMLUtilities::ReadElementFromFile(styluscalibratedConfigFileName.c_str()));
  if (configStylusCalibration == NULL)
  {  
    LOG_ERROR("Unable to read StylusCalibration configuration from file " << styluscalibratedConfigFileName.c_str()); 
    exit(EXIT_FAILURE);
  }
  // Read stylus tracker data
  vtkSmartPointer<vtkTrackedFrameList> trackedStylusTipFrames = vtkSmartPointer<vtkTrackedFrameList>::New();
  if( !inputTrackedStylusTipSequenceMetafile.empty() )
  {
    trackedStylusTipFrames->SetValidationRequirements(REQUIRE_UNIQUE_TIMESTAMP | REQUIRE_TRACKING_OK);
  }
  LOG_INFO("Read stylus tracker data from " << inputTrackedStylusTipSequenceMetafile);
  if ( trackedStylusTipFrames->ReadFromSequenceMetafile(inputTrackedStylusTipSequenceMetafile.c_str()) != PLUS_SUCCESS )
  {
    LOG_ERROR("Failed to read stylus data from sequence metafile: " << inputTrackedStylusTipSequenceMetafile << ". Exiting...");
    exit(EXIT_FAILURE);
  }
  trackedStylusTipFrames->Register(NULL);
  ConstructSignalPlot( trackedStylusTipFrames, intermediateFileOutputDirectory, configStylusCalibration);

  //--------------------------------------------------------------------------------------------------------------------------------------
  // Initialize data collection
  vtkSmartPointer<vtkDataCollector> dataCollector = vtkSmartPointer<vtkDataCollector>::New(); 
  if (dataCollector->ReadConfiguration(configPivotDetection) != PLUS_SUCCESS) {
    LOG_ERROR("Unable to parse configuration from file " << inputConfigFileName.c_str()); 
    exit(EXIT_FAILURE);
  }
  if (dataCollector->Connect() != PLUS_SUCCESS)
  {
    LOG_ERROR("Data collector was unable to connect to devices!");
    exit(EXIT_FAILURE);
  }
  if (dataCollector->Start() != PLUS_SUCCESS)
  {
    LOG_ERROR("Unable to start data collection!");
    exit(EXIT_FAILURE);
  }
  vtkPlusChannel* aChannel(NULL);
  vtkPlusDevice* aDevice(NULL);
  if( dataCollector->GetDevice(aDevice, std::string("TrackerDevice")) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to locate device by ID: \'TrackerDevice\'");
    exit(EXIT_FAILURE);
  }
  if( aDevice->GetOutputChannelByName(aChannel, "TrackerStream") != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to locate channel by ID: \'TrackerStream\'");
    exit(EXIT_FAILURE);
  }
  if ( aChannel->GetTrackingDataAvailable() == false ) {
    LOG_ERROR("Channel \'" << aChannel->GetChannelId() << "\' is not tracking!");
    exit(EXIT_FAILURE);
  }
  if (aChannel->GetTrackingDataAvailable() == false)
  {
    LOG_ERROR("Data collector is not tracking!");
    exit(EXIT_FAILURE);
  }
  // Read coordinate definitions
  vtkSmartPointer<vtkTransformRepository> transformRepository = vtkSmartPointer<vtkTransformRepository>::New();
  if ( transformRepository->ReadConfiguration(configPivotDetection) != PLUS_SUCCESS )
  {
    LOG_ERROR("Failed to read CoordinateDefinitions!"); 
    exit(EXIT_FAILURE);
  }
  // Initialize phantom registration
  vtkSmartPointer<vtkPhantomLandmarkRegistrationAlgo> phantomRegistration = vtkSmartPointer<vtkPhantomLandmarkRegistrationAlgo>::New();
  if (phantomRegistration == NULL)
  {
    LOG_ERROR("Unable to instantiate phantom registration algorithm class!");
    exit(EXIT_FAILURE);
  }
  if (phantomRegistration->ReadConfiguration(configPivotDetection) != PLUS_SUCCESS)
  {
    LOG_ERROR("Unable to read phantom definition!");
    exit(EXIT_FAILURE);
  }
  int numberOfLandmarks = phantomRegistration->GetDefinedLandmarks()->GetNumberOfPoints();
  if (numberOfLandmarks != 8)
  {
    LOG_ERROR("Number of defined landmarks should be 8 instead of " << numberOfLandmarks << "!");
    exit(EXIT_FAILURE);
  }
  // Acquire landmarks
  vtkFakeTracker *fakeTracker = dynamic_cast<vtkFakeTracker*>(aDevice);
  if (fakeTracker == NULL) {
    LOG_ERROR("Invalid tracker object!");
    exit(EXIT_FAILURE);
  }
  fakeTracker->SetTransformRepository(transformRepository);
  TrackedFrame trackedFrame;
  PlusTransformName stylusTipToReferenceTransformName(phantomRegistration->GetStylusTipCoordinateFrame(), phantomRegistration->GetReferenceCoordinateFrame());
  ///-----------------------------------------------------------------------------------------------------------------
  vtkSmartPointer<vtkPivotDetectionAlgo> PivotDetection = vtkSmartPointer<vtkPivotDetectionAlgo>::New();
  if (PivotDetection == NULL)
  {
    LOG_ERROR("Unable to instantiate pivot detection algorithm class!");
    exit(EXIT_FAILURE);
  }

  if (PivotDetection->ReadConfiguration(configPivotDetection) != PLUS_SUCCESS)
  {
    LOG_ERROR("Unable to read pivot calibration configuration!");
    exit(EXIT_FAILURE);
  }

  // Check stylus tool
  PlusTransformName stylusToReferenceTransformName(PivotDetection->GetObjectMarkerCoordinateFrame(), PivotDetection->GetReferenceCoordinateFrame());
  PlusTransformName stylusTipToStylusTransformName(phantomRegistration->GetStylusTipCoordinateFrame(), PivotDetection->GetObjectMarkerCoordinateFrame());
  vtkSmartPointer<vtkTransformRepository> transformRepositoryCalibration = vtkSmartPointer<vtkTransformRepository>::New();
  if ( transformRepositoryCalibration->ReadConfiguration(configStylusCalibration) != PLUS_SUCCESS )
  {
    LOG_ERROR("Failed to read CoordinateDefinitions!"); 
    exit(EXIT_FAILURE);
  }
  vtkSmartPointer<vtkMatrix4x4> stylusTipToStylusTransform = vtkSmartPointer<vtkMatrix4x4>::New();
  bool valid = false;
  transformRepositoryCalibration->GetTransform(stylusTipToStylusTransformName, stylusTipToStylusTransform, &valid);
  double offsetPhantom[3] = {0,0,0};
  double pivotFound[3] = {0,0,0};
  double pivotLandmark[3] = {0,0,0};
  if (valid)
  {
    // Acquire positions for pivot detection
    for (int i=0; i < trackedStylusTipFrames->GetNumberOfTrackedFrames(); ++i)
    {
      fakeTracker->SetCounter(i);
      vtkAccurateTimer::Delay(2.1 / fakeTracker->GetAcquisitionRate());
      //aChannel->GetTrackedFrame((trackedStylusTipFrames->GetTrackedFrame(i)));

      vtkSmartPointer<vtkMatrix4x4> stylusToReferenceMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
      if ( transformRepository->SetTransforms(*(trackedStylusTipFrames->GetTrackedFrame(i))) != PLUS_SUCCESS )
      {
        LOG_ERROR("Failed to update transforms in repository with tracked frame!");
        exit(EXIT_FAILURE);
      }

      valid = false;
      if ( (transformRepository->GetTransform(stylusToReferenceTransformName, stylusToReferenceMatrix, &valid) != PLUS_SUCCESS) || (!valid) )
      {
        LOG_ERROR("No valid transform found between stylus to reference!");
        exit(EXIT_FAILURE);
      }
      vtkMatrix4x4* stylusTipToReferenceTransformMatrix = vtkMatrix4x4::New();
      vtkMatrix4x4::Multiply4x4(stylusToReferenceMatrix,stylusTipToStylusTransform,stylusTipToReferenceTransformMatrix);
      PivotDetection->InsertNextDetectionPoint(stylusTipToReferenceTransformMatrix);

      if(PivotDetection->PivotFound()==PLUS_SUCCESS)
      {
        vtkPlusLogger::PrintProgressbar((100.0 * PivotDetection->GetPivotPointsReference()->GetNumberOfPoints()-1) / NUMBER_PIVOTS); 
        PivotDetection->GetPivotPointsReference()->GetPoint(PivotDetection->GetPivotPointsReference()->GetNumberOfPoints()-1, pivotFound);
        // Add recorded point to algorithm
        phantomRegistration->GetRecordedLandmarks()->InsertPoint(PivotDetection->GetPivotPointsReference()->GetNumberOfPoints()-1, pivotFound);
        phantomRegistration->GetRecordedLandmarks()->Modified();
        LOG_INFO("\nPivot found (" << pivotFound[0]<<", " << pivotFound[1]<<", " << pivotFound[2]<<") at "<<trackedStylusTipFrames->GetTrackedFrame(i)->GetTimestamp()<<"[ms]"<< "\nNumber of pivots in phantonReg "<<phantomRegistration->GetRecordedLandmarks()->GetNumberOfPoints());
      }

      if(PivotDetection->GetPivotPointsReference()->GetNumberOfPoints()==NUMBER_PIVOTS)
      {
        break;
      }
    }
  }
  else
  {
    LOG_ERROR("No valid transform found between stylus to stylus tip!");
  }

  LOG_INFO(PivotDetection->GetDetectedPivotsString());

  //for (int id =0; id<PivotDetection->GetPivotPointsReference()->GetNumberOfPoints();id++)
  //{
  //  // Compare to baseline
  //  phantomRegistration->GetDefinedLandmarks()->GetPoint(id, pivotLandmark);
  //  //phantomRegistration->GetRecordedLandmarks()->GetPoint(id, pivotLandmark);
  //  PivotDetection->GetPivotPointsReference()->GetPoint(id, pivotFound);
  //  //pivotLandmark[0]=pivotFound[0]-pivotLandmark[0];
  //  //pivotLandmark[1]=pivotFound[1]-pivotLandmark[1];
  //  //pivotLandmark[2]=pivotFound[2]-pivotLandmark[2];
  //  //if (vtkMath::Norm(pivotLandmark)>ERROR_THRESHOLD_MM)
  //  //{
  //  //  LOG_ERROR("Comparison of calibration data to baseline failed");
  //  //  std::cout << "Exit failure!!!" << std::endl;
  //  //  return EXIT_FAILURE;
  //  //}
  //  //else
  //  //{
  //    LOG_INFO("\nPivot "<< id << " found (" << pivotFound[0]<<", " << pivotFound[1]<<", " << pivotFound[2]);
  //    LOG_INFO("\nDefLandmark "<< id << " (" << pivotLandmark[0]<<", " << pivotLandmark[1]<<", " << pivotLandmark[2]);
  //  //}
  //}

  if (phantomRegistration->Register(transformRepository) != PLUS_SUCCESS)
  {
    LOG_ERROR("Phantom registration failed!");
    exit(EXIT_FAILURE);
  }

  //PlusTransformName phantomToReferenceTransformName(phantomRegistration->PhantomCoordinateFrame, phantomRegistration->ReferenceCoordinateFrame);
  //transformRepository->GetTransform(phantomToReferenceTransformName);
  vtkPlusLogger::PrintProgressbar(100); 

  LOG_INFO("Registration error = " << phantomRegistration->GetRegistrationError());

  // Save result
  if (transformRepository->WriteConfiguration(configPivotDetection) != PLUS_SUCCESS )
  {
    LOG_ERROR("Failed to write phantom registration result to configuration element!");
    exit(EXIT_FAILURE);
  }

  std::string registrationResultFileName = "PhantomRegistrationAutoDetectPivotingTest.xml";
  vtksys::SystemTools::RemoveFile(registrationResultFileName.c_str());
  PlusCommon::PrintXML(registrationResultFileName.c_str(), configPivotDetection); 

  if ( CompareRegistrationResultsWithBaseline( inputBaselineFileName.c_str(), registrationResultFileName.c_str(), phantomRegistration->GetPhantomCoordinateFrame(), phantomRegistration->GetReferenceCoordinateFrame() ) != PLUS_SUCCESS )
  {
    LOG_ERROR("Comparison of calibration data to baseline failed");
    std::cout << "Exit failure!!!" << std::endl; 
    return EXIT_FAILURE;
  }

  std::cout << "Exit success!!!" << std::endl; 
  return EXIT_SUCCESS; 
}

//-----------------------------------------------------------------------------

// return the number of differences
PlusStatus CompareRegistrationResultsWithBaseline(const char* baselineFileName, const char* currentResultFileName, const char* phantomCoordinateFrame, const char* referenceCoordinateFrame)
{
  int numberOfFailures=0;

  if ( baselineFileName == NULL )
  {
    LOG_ERROR("Unable to read the baseline configuration file - filename is NULL"); 
    return PLUS_FAIL;
  }

  if ( currentResultFileName == NULL )
  {
    LOG_ERROR("Unable to read the current configuration file - filename is NULL"); 
    return PLUS_FAIL;
  }

  PlusTransformName tnPhantomToPhantomReference(phantomCoordinateFrame, referenceCoordinateFrame); 

  // Load current phantom registration
  vtkSmartPointer<vtkXMLDataElement> currentRootElem = vtkSmartPointer<vtkXMLDataElement>::Take(
    vtkXMLUtilities::ReadElementFromFile(currentResultFileName));
  if (currentRootElem == NULL) 
  {  
    LOG_ERROR("Unable to read the current configuration file: " << currentResultFileName); 
    return PLUS_FAIL;
  }

  vtkSmartPointer<vtkTransformRepository> currentTransformRepository = vtkSmartPointer<vtkTransformRepository>::New(); 
  if ( currentTransformRepository->ReadConfiguration(currentRootElem) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to read the current CoordinateDefinitions from configuration file: " << currentResultFileName); 
    return PLUS_FAIL;
  }

  vtkSmartPointer<vtkMatrix4x4> currentMatrix = vtkSmartPointer<vtkMatrix4x4>::New(); 
  if ( currentTransformRepository->GetTransform(tnPhantomToPhantomReference, currentMatrix) != PLUS_SUCCESS )
  {
    std::string strTransformName; 
    tnPhantomToPhantomReference.GetTransformName(strTransformName);
    LOG_ERROR("Unable to get '" << strTransformName << "' coordinate definition from configuration file: " << currentResultFileName); 
    return PLUS_FAIL;
  }

  // Load baseline phantom registration
  vtkSmartPointer<vtkXMLDataElement> baselineRootElem = vtkSmartPointer<vtkXMLDataElement>::Take(
    vtkXMLUtilities::ReadElementFromFile(baselineFileName));
  if (baselineFileName == NULL) 
  {  
    LOG_ERROR("Unable to read the baseline configuration file: " << baselineFileName); 
    return PLUS_FAIL;
  }

  vtkSmartPointer<vtkTransformRepository> baselineTransformRepository = vtkSmartPointer<vtkTransformRepository>::New(); 
  if ( baselineTransformRepository->ReadConfiguration(baselineRootElem) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to read the baseline CoordinateDefinitions from configuration file: " << baselineFileName); 
    return PLUS_FAIL;
  }

  vtkSmartPointer<vtkMatrix4x4> baselineMatrix = vtkSmartPointer<vtkMatrix4x4>::New(); 
  if ( baselineTransformRepository->GetTransform(tnPhantomToPhantomReference, baselineMatrix) != PLUS_SUCCESS )
  {
    std::string strTransformName; 
    tnPhantomToPhantomReference.GetTransformName(strTransformName);
    LOG_ERROR("Unable to get '" << strTransformName << "' coordinate definition from configuration file: " << baselineFileName); 
    return PLUS_FAIL;
  }

  // Compare the transforms
  double posDiff=PlusMath::GetPositionDifference(currentMatrix, baselineMatrix); 
  double orientDiff=PlusMath::GetOrientationDifference(currentMatrix, baselineMatrix); 

  if ( fabs(posDiff) > ERROR_THRESHOLD_MM || fabs(orientDiff) > ERROR_THRESHOLD_MM )
  {
    LOG_ERROR("Transform mismatch (position difference: " << posDiff << "  orientation difference: " << orientDiff);
    return PLUS_FAIL; 
  }


  //if (trackedStylusTipFrames != NULL)
  //{
  //  trackedStylusTipFrames->UnRegister(NULL);
  //  trackedStylusTipFrames = NULL;
  //}

  return PLUS_SUCCESS; 
}
