/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#include "PlusConfigure.h"
#include "vtksys/CommandLineArguments.hxx"
#include "vtkDataCollectorHardwareDevice.h"
#include "vtkDataCollectorSynchronizer.h"
#include "vtkTracker.h"
#include "vtkTrackerTool.h"
#include "vtkVideoBuffer.h"
#include "vtkHTMLGenerator.h"
#include "vtkGnuplotExecuter.h"
#include "vtksys/SystemTools.hxx"
#include "vtkXMLUtilities.h"
#include "vtkTimerLog.h"
#include "vtkPlusVideoSource.h"

int main(int argc, char **argv)
{
	bool printHelp(false);
	std::string inputConfigFileName;
	double inputAcqTimeLength(60);
	std::string outputFolder("./"); 
	std::string outputTrackerBufferSequenceFileName("TrackerBufferMetafile"); 
	std::string outputVideoBufferSequenceFileName("VideoBufferMetafile"); 
	int numberOfAveragedFrames(15); 
	int numberOfAveragedTransforms(20); 
	double thresholdMultiplier(5);
  std::string toolName("Probe");

	int verboseLevel=vtkPlusLogger::LOG_LEVEL_DEFAULT;

	vtksys::CommandLineArguments args;
	args.Initialize(argc, argv);

	args.AddArgument("--help", vtksys::CommandLineArguments::NO_ARGUMENT, &printHelp, "Print this help.");	
	args.AddArgument("--input-config-file-name", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputConfigFileName, "Name of the input configuration file.");
	args.AddArgument("--input-acq-time-length", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputAcqTimeLength, "Length of acquisition time in seconds (Default: 60s)");	
	args.AddArgument("--output-tracker-buffer-seq-file-name", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &outputTrackerBufferSequenceFileName, "Filename of the output tracker buffer sequence metafile (Default: TrackerBufferMetafile)");
	args.AddArgument("--output-video-buffer-seq-file-name", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &outputVideoBufferSequenceFileName, "Filename of the output video buffer sequence metafile (Default: VideoBufferMetafile)");
	args.AddArgument("--output-folder", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &outputFolder, "Output folder (Default: ./)");
	args.AddArgument("--averaged-frames", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &numberOfAveragedFrames, "Number of averaged frames for synchronization (Default: 15)");
	args.AddArgument("--averaged-transforms", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &numberOfAveragedTransforms, "Number of averaged transforms for synchronization (Default: 20)");
	args.AddArgument("--threshold-multiplier", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &thresholdMultiplier, "Set the stdev multiplier of threshold value for synchronization (Default: 5)");
	args.AddArgument("--tool-name", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &toolName, "Name of the used tool (Default: Probe)");

	args.AddArgument("--verbose", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &verboseLevel, "Verbose level (1=error only, 2=warning, 3=info, 4=debug, 5=trace)");	

	vtkPlusLogger::Instance()->SetLogLevel(verboseLevel);

	if ( !args.Parse() )
	{
		std::cerr << "Problem parsing arguments" << std::endl;
		std::cout << "Help: " << args.GetHelp() << std::endl;
		exit(EXIT_FAILURE);
	}

	if ( printHelp ) 
	{
		std::cout << "Help: " << args.GetHelp() << std::endl;
		exit(EXIT_SUCCESS); 

	}

	if (inputConfigFileName.empty())
	{
		std::cerr << "input-config-file-name is required" << std::endl;
		exit(EXIT_FAILURE);
	}

	///////////////

	//************************************************************************************
	// Find program path 
	std::string programPath("./"), errorMsg; 
	if ( !vtksys::SystemTools::FindProgramPath(argv[0], programPath, errorMsg) )
	{
		LOG_ERROR(errorMsg); 
	}
	programPath = vtksys::SystemTools::GetParentDirectory(programPath.c_str()); 

	//************************************************************************************
	// Initialize data collector
  vtkSmartPointer<vtkXMLDataElement> configRootElement = vtkSmartPointer<vtkXMLDataElement>::Take(vtkXMLUtilities::ReadElementFromFile(inputConfigFileName.c_str()));
  if (configRootElement == NULL) {	
    LOG_ERROR("Unable to read configuration from file " << inputConfigFileName); 
		exit(EXIT_FAILURE);
  }

  vtkPlusConfig::GetInstance()->SetDeviceSetConfigurationData(configRootElement);

  vtkSmartPointer<vtkDataCollectorHardwareDevice> dataCollectorHardwareDevice = vtkSmartPointer<vtkDataCollectorHardwareDevice>::New(); 
  if ( dataCollectorHardwareDevice == NULL )
  {
    LOG_ERROR("Failed to create data collector!");
    exit(EXIT_FAILURE);
  }

  dataCollectorHardwareDevice->ReadConfiguration(configRootElement);
	dataCollectorHardwareDevice->Connect();
	dataCollectorHardwareDevice->Start();

	const double acqStartTime = vtkTimerLog::GetUniversalTime(); 

	//************************************************************************************
	// Record data
	while ( acqStartTime + inputAcqTimeLength > vtkTimerLog::GetUniversalTime() )
	{
		LOG_INFO( acqStartTime + inputAcqTimeLength - vtkTimerLog::GetUniversalTime() << " seconds left..." ); 
		vtksys::SystemTools::Delay(1000); 
	}

	
	//************************************************************************************
	// Copy buffers to local buffer
	vtkSmartPointer<vtkVideoBuffer> videobuffer = vtkSmartPointer<vtkVideoBuffer>::New(); 
	if ( dataCollectorHardwareDevice->GetVideoSource() != NULL ) 
	{
		LOG_INFO("Copy video buffer ..."); 
    videobuffer->DeepCopy(dataCollectorHardwareDevice->GetVideoSource()->GetBuffer()); 
	}

	vtkSmartPointer<vtkTracker> tracker = vtkSmartPointer<vtkTracker>::New(); 
	if ( dataCollectorHardwareDevice->GetTracker() != NULL )
	{
		LOG_INFO("Copy tracker ..."); 
		tracker->DeepCopy(dataCollectorHardwareDevice->GetTracker()); 
	}

  vtkTrackerTool* tool = NULL;
  if (dataCollectorHardwareDevice->GetTracker()->GetTool(toolName.c_str(), tool) != PLUS_SUCCESS)
  {
    LOG_ERROR("No tool found with name '" << toolName << "'");
		exit(EXIT_FAILURE);
  }
	vtkTrackerBuffer* trackerbuffer = tool->GetBuffer(); 

	//************************************************************************************
	// Stop recording
	if ( dataCollectorHardwareDevice->GetVideoSource() != NULL ) 
	{
		LOG_INFO("Stop video recording ..."); 
		dataCollectorHardwareDevice->GetVideoSource()->StopRecording(); 
	}

	if ( dataCollectorHardwareDevice->GetTracker() != NULL )
	{
		LOG_INFO("Stop tracking ..."); 
		dataCollectorHardwareDevice->GetTracker()->StopTracking(); 
	}

	//************************************************************************************
	// Run synchronizer 
	LOG_INFO("Initialize synchronizer..."); 
	vtkSmartPointer<vtkDataCollectorSynchronizer> synchronizer = vtkSmartPointer<vtkDataCollectorSynchronizer>::New(); 
	synchronizer->SetSynchronizationTimeLength(inputAcqTimeLength); 
	synchronizer->SetNumberOfAveragedFrames(numberOfAveragedFrames); 
	synchronizer->SetNumberOfAveragedTransforms(numberOfAveragedTransforms); 
	synchronizer->SetThresholdMultiplier(thresholdMultiplier); 
	synchronizer->SetTrackerBuffer(trackerbuffer); 
	synchronizer->SetVideoBuffer(videobuffer); 

	LOG_INFO("Number Of Averaged Frames: " << numberOfAveragedFrames); 
	LOG_INFO("Number Of Averaged Transforms: " << numberOfAveragedTransforms ); 
	LOG_INFO("Threshold Multiplier: " << thresholdMultiplier ); 
	LOG_INFO("Tracker Buffer Size: " << trackerbuffer->GetNumberOfItems() ); 
	LOG_INFO("Tracker Frame Rate: " << trackerbuffer->GetFrameRate() ); 
	LOG_INFO("Video Buffer Size: " << videobuffer->GetNumberOfItems() ); 
	LOG_INFO("Video Frame Rate: " << videobuffer->GetFrameRate() ); 

	synchronizer->Synchronize(); 

	//************************************************************************************
	// Generate html report
	LOG_INFO("Generate report ...");
	vtkSmartPointer<vtkHTMLGenerator> htmlReport = vtkSmartPointer<vtkHTMLGenerator>::New(); 
	htmlReport->SetTitle("iCAL Temporal Calibration Report"); 

	vtkSmartPointer<vtkGnuplotExecuter> plotter = vtkSmartPointer<vtkGnuplotExecuter>::New(); 
	plotter->SetHideWindow(true); 

	// Generate tracking data acq report
	tracker->GenerateTrackingDataAcquisitionReport(htmlReport, plotter); 

	// Generate video data acq report
	dataCollectorHardwareDevice->GetVideoSource()->GenerateVideoDataAcquisitionReport(htmlReport, plotter); 

	// Synchronizer Analysis report
	synchronizer->GenerateSynchronizationReport(htmlReport, plotter); 

	htmlReport->SaveHtmlPage("iCALTemporalCalibrationReport.html"); 
	//************************************************************************************

	// Dump buffers to file 
	if ( dataCollectorHardwareDevice->GetVideoSource() != NULL ) 
	{
		LOG_INFO("Write video buffer to " << outputVideoBufferSequenceFileName);
		dataCollectorHardwareDevice->GetVideoSource()->GetBuffer()->WriteToMetafile( outputFolder.c_str(), outputVideoBufferSequenceFileName.c_str(), false); 
	}

	if ( dataCollectorHardwareDevice->GetTracker() != NULL )
	{
		LOG_INFO("Write tracker buffer to " << outputTrackerBufferSequenceFileName);
		tracker->WriteToMetafile( outputFolder.c_str(), outputTrackerBufferSequenceFileName.c_str(), false); 
	}	

	return EXIT_SUCCESS; 
}
