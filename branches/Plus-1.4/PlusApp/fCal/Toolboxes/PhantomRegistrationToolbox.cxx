/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/ 

#include "PhantomRegistrationToolbox.h"

#include "fCalMainWindow.h"
#include "vtkObjectVisualizer.h"
#include "vtkDataCollectorHardwareDevice.h"

#include "ConfigFileSaverDialog.h"

#include "vtkPhantomRegistrationAlgo.h"
#include "vtkPivotCalibrationAlgo.h"
#include "vtkFakeTracker.h"

#include <QFileDialog>

#include "vtkXMLUtilities.h"
#include "vtkRenderWindow.h"
#include "vtkAccurateTimer.h"
#include "vtkPolyDataMapper.h"
#include "vtkGlyph3D.h"
#include "vtkSphereSource.h"
#include "vtkProperty.h"
#include "vtkSTLReader.h"

//-----------------------------------------------------------------------------

PhantomRegistrationToolbox::PhantomRegistrationToolbox(fCalMainWindow* aParentMainWindow, Qt::WFlags aFlags)
  : AbstractToolbox(aParentMainWindow)
  , QWidget(aParentMainWindow, aFlags)
  , m_PhantomActor(NULL)
  , m_RequestedLandmarkActor(NULL)
  , m_RequestedLandmarkPolyData(NULL)
  , m_PhantomRenderer(NULL)
  , m_CurrentLandmarkIndex(0)
{
  ui.setupUi(this);

  // Create algorithm class
  m_PhantomRegistration = vtkPhantomRegistrationAlgo::New();
  if (m_PhantomRegistration == NULL)
  {
    LOG_ERROR("Unable to instantiate phantom registration algorithm class!");
    return;
  }

  // Create and add renderer to phantom canvas
  m_PhantomRenderer = vtkRenderer::New();
  m_PhantomRenderer->SetBackground(0.1, 0.1, 0.1);
  m_PhantomRenderer->SetBackground2(0.4, 0.4, 0.4);
  m_PhantomRenderer->SetGradientBackground(true);

  ui.canvasPhantom->GetRenderWindow()->AddRenderer(m_PhantomRenderer);

  // Connect events
  connect( ui.pushButton_OpenStylusCalibration, SIGNAL( clicked() ), this, SLOT( OpenStylusCalibration() ) );
  connect( ui.pushButton_RecordPoint, SIGNAL( clicked() ), this, SLOT( RecordPoint() ) );
  connect( ui.pushButton_Undo, SIGNAL( clicked() ), this, SLOT( Undo() ) );
  connect( ui.pushButton_Reset, SIGNAL( clicked() ), this, SLOT( Reset() ) );
}

//-----------------------------------------------------------------------------

PhantomRegistrationToolbox::~PhantomRegistrationToolbox()
{
  if (m_PhantomRegistration != NULL) {
    m_PhantomRegistration->Delete();
    m_PhantomRegistration = NULL;
  } 

   if (m_PhantomActor != NULL) {
    m_PhantomRenderer->RemoveActor(m_PhantomActor);
    m_PhantomActor->Delete();
    m_PhantomActor = NULL;
  }

  if (m_RequestedLandmarkActor != NULL) {
    m_PhantomRenderer->RemoveActor(m_RequestedLandmarkActor);
    m_RequestedLandmarkActor->Delete();
    m_RequestedLandmarkActor = NULL;
  }

  if (m_RequestedLandmarkPolyData != NULL) {
    m_RequestedLandmarkPolyData->Delete();
    m_RequestedLandmarkPolyData = NULL;
  }

  if (m_PhantomRenderer != NULL) {
    ui.canvasPhantom->GetRenderWindow()->RemoveRenderer(m_PhantomRenderer);
    m_PhantomRenderer->Delete();
    m_PhantomRenderer = NULL;
  }
}

//-----------------------------------------------------------------------------

void PhantomRegistrationToolbox::Initialize()
{
  LOG_TRACE("PhantomRegistrationToolbox::Initialize"); 

  if (m_State == ToolboxState_Done)
  {
    SetDisplayAccordingToState();
    return;
  }

  if ( (m_ParentMainWindow->GetObjectVisualizer()->GetDataCollector() != NULL)
    && (m_ParentMainWindow->GetObjectVisualizer()->GetDataCollector()->GetConnected()) )
  {
    //m_ParentMainWindow->GetObjectVisualizer()->GetDataCollector()->SetTrackingOnly(true);

    if (m_PhantomRegistration->ReadConfiguration(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData()) != PLUS_SUCCESS)
    {
      LOG_ERROR("Reading phantom registration algorithm configuration failed!");
      return;
    }

    if (ReadConfiguration(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData()) != PLUS_SUCCESS)
    {
      LOG_ERROR("Stylus tool name cannot be loaded from device set configuration data!");
      return;
    }

    // Check if stylus tip to reference transform is available
    if (m_ParentMainWindow->GetObjectVisualizer()->IsExistingTransform(m_PhantomRegistration->GetStylusTipCoordinateFrame(), m_PhantomRegistration->GetReferenceCoordinateFrame()) == PLUS_SUCCESS)
    {
      // Set to InProgress if both stylus calibration and phantom definition are available
      Start();
    }
    else
    {
      ui.label_Instructions->setText(tr("Stylus calibration needs to be imported"));
    }

    // Set state to idle
    if (m_State == ToolboxState_Uninitialized)
    {
      SetState(ToolboxState_Idle);
    }
    else
    {
      SetDisplayAccordingToState();
    }
  }
  else
  {
    SetState(ToolboxState_Uninitialized);
  }
}

//-----------------------------------------------------------------------------

PlusStatus PhantomRegistrationToolbox::ReadConfiguration(vtkXMLDataElement* aConfig)
{
  LOG_TRACE("PhantomRegistrationToolbox::ReadConfiguration");

  if (aConfig == NULL)
  {
    LOG_ERROR("Unable to read configuration"); 
    return PLUS_FAIL; 
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus PhantomRegistrationToolbox::InitializeVisualization()
{
  LOG_TRACE("PhantomRegistrationToolbox::InitializeVisualization"); 

  if (m_State == ToolboxState_Uninitialized)
  {
    vtkDisplayableObject* phantomDisplayableObject = NULL;
    if ( m_PhantomRegistration->GetPhantomCoordinateFrame() == NULL
      || m_ParentMainWindow->GetObjectVisualizer()->GetDisplayableObject(m_PhantomRegistration->GetPhantomCoordinateFrame(), phantomDisplayableObject) != PLUS_SUCCESS )
    {
      LOG_ERROR("Unable to get phantom displayable object!");
      return PLUS_FAIL;
    }

    // Initialize requested landmarks visualization in toolbox canvas
    m_RequestedLandmarkPolyData = vtkPolyData::New();
    m_RequestedLandmarkPolyData->Initialize();
    vtkSmartPointer<vtkPoints> requestedLandmarkPoints = vtkSmartPointer<vtkPoints>::New();
    m_RequestedLandmarkPolyData->SetPoints(requestedLandmarkPoints);

    m_RequestedLandmarkActor = vtkActor::New();
    vtkSmartPointer<vtkPolyDataMapper> requestedLandmarksMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    vtkSmartPointer<vtkGlyph3D> requestedLandmarksGlyph = vtkSmartPointer<vtkGlyph3D>::New();
    vtkSmartPointer<vtkSphereSource> requestedLandmarksSphereSource = vtkSmartPointer<vtkSphereSource>::New();
    requestedLandmarksSphereSource->SetRadius(1.5); // mm

    requestedLandmarksGlyph->SetInputConnection(m_RequestedLandmarkPolyData->GetProducerPort());
    requestedLandmarksGlyph->SetSourceConnection(requestedLandmarksSphereSource->GetOutputPort());
    requestedLandmarksMapper->SetInputConnection(requestedLandmarksGlyph->GetOutputPort());
    m_RequestedLandmarkActor->SetMapper(requestedLandmarksMapper);
    m_RequestedLandmarkActor->GetProperty()->SetColor(1.0, 0.0, 0.0);

    // Initialize phantom visualization in toolbox canvas
    vtkDisplayableModel* phantomDisplayableModel = dynamic_cast<vtkDisplayableModel*>(phantomDisplayableObject);
    if ( phantomDisplayableModel->GetSTLModelFileName() != NULL && phantomDisplayableModel->GetModelToObjectTransform() != NULL )
    {
      std::string searchResult = vtkPlusConfig::GetFirstFileFoundInConfigurationDirectory(phantomDisplayableModel->GetSTLModelFileName());
      if (STRCASECMP("", searchResult.c_str()) == 0)
      {
        LOG_ERROR("Failed to find phantom model file in configuration directory!");
        return PLUS_FAIL;
      }
      
      vtkSmartPointer<vtkSTLReader> stlReader = vtkSmartPointer<vtkSTLReader>::New();
      m_PhantomActor = vtkActor::New();
      vtkSmartPointer<vtkPolyDataMapper> stlMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
      stlReader->SetFileName(searchResult.c_str());
      stlMapper->SetInputConnection(stlReader->GetOutputPort());
      m_PhantomActor->SetMapper(stlMapper);
      m_PhantomActor->GetProperty()->SetOpacity( phantomDisplayableModel->GetLastOpacity() );
      m_PhantomActor->SetUserTransform(phantomDisplayableModel->GetModelToObjectTransform());
    }
    else
    {
      LOG_ERROR("Phantom cannot be visualized in toolbox canvas because model or model to object transform is invalid!");
      return PLUS_FAIL;
    }

    // Add actors
    m_PhantomRenderer->AddActor(m_PhantomActor);
    m_PhantomRenderer->AddActor(m_RequestedLandmarkActor);

    m_PhantomRenderer->ResetCamera();
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

void PhantomRegistrationToolbox::RefreshContent()
{
  //LOG_TRACE("PhantomRegistrationToolbox::RefreshContent"); 

  // If in progress
  if (m_State == ToolboxState_InProgress)
  {
    ui.label_Instructions->setText(QString("Touch landmark named %1 and press Record point button").arg(m_PhantomRegistration->GetDefinedLandmarkName(m_CurrentLandmarkIndex).c_str()));

    if (m_CurrentLandmarkIndex < 1)
    {
      ui.pushButton_Undo->setEnabled(false);
      ui.pushButton_Reset->setEnabled(false);
    }
    else
    {
      ui.pushButton_Undo->setEnabled(true);
      ui.pushButton_Reset->setEnabled(true);
    }

    m_ParentMainWindow->SetStatusBarProgress((int)(100.0 * (m_CurrentLandmarkIndex / m_PhantomRegistration->GetDefinedLandmarks()->GetNumberOfPoints()) + 0.5));

  }

  if (m_State == ToolboxState_Done || m_State == ToolboxState_InProgress)
  {
    // Get stylus tip position and display it
    std::string stylusTipPosition;
    bool valid = false;
    if (m_ParentMainWindow->GetObjectVisualizer()->GetTransformTranslationString(m_PhantomRegistration->GetStylusTipCoordinateFrame(), m_PhantomRegistration->GetReferenceCoordinateFrame(), stylusTipPosition, &valid) != PLUS_SUCCESS)
    {
      LOG_ERROR("Unable to get stylus tip to reference transform!");
      return;
    }

    if (valid)
    {
      ui.label_StylusPosition->setText(QString(stylusTipPosition.c_str()));
    }
    else
    {
      ui.label_StylusPosition->setText(tr("Stylus is out of view"));
    }
  }

  ui.canvasPhantom->update();
}

//-----------------------------------------------------------------------------

void PhantomRegistrationToolbox::SetDisplayAccordingToState()
{
  LOG_TRACE("PhantomRegistrationToolbox::SetDisplayAccordingToState");

  if (m_ParentMainWindow->AreDevicesShown() == false)
  {
    m_ParentMainWindow->GetObjectVisualizer()->EnableImageMode(false);
    m_ParentMainWindow->GetObjectVisualizer()->HideAll();
  }

  if (m_State == ToolboxState_Uninitialized)
  {
    ui.label_StylusPosition->setText(tr("N/A"));
    ui.label_Instructions->setText("");

    ui.pushButton_OpenStylusCalibration->setEnabled(false);
    ui.pushButton_RecordPoint->setEnabled(false);
    ui.pushButton_Reset->setEnabled(false);
    ui.pushButton_Undo->setEnabled(false);

    m_ParentMainWindow->SetStatusBarText(QString(""));
    m_ParentMainWindow->SetStatusBarProgress(-1);
  }
  else if (m_State == ToolboxState_Idle)
  {
    ui.label_StylusPosition->setText(tr("N/A"));

    ui.pushButton_OpenStylusCalibration->setEnabled(true);
    ui.pushButton_RecordPoint->setEnabled(false);
    ui.pushButton_Reset->setEnabled(false);
    ui.pushButton_Undo->setEnabled(false);

    m_ParentMainWindow->SetStatusBarText(QString(""));
    m_ParentMainWindow->SetStatusBarProgress(-1);
  }
  else if (m_State == ToolboxState_InProgress)
  {
    ui.pushButton_OpenStylusCalibration->setEnabled(true);
    ui.pushButton_RecordPoint->setEnabled(true);

    if (m_CurrentLandmarkIndex < 1) {
      ui.pushButton_Undo->setEnabled(false);
      ui.pushButton_Reset->setEnabled(false);
    } else {
      ui.pushButton_Undo->setEnabled(true);
      ui.pushButton_Reset->setEnabled(true);
    }

    m_ParentMainWindow->SetStatusBarText(QString(" Recording phantom landmarks"));
    m_ParentMainWindow->SetStatusBarProgress(0);

    m_ParentMainWindow->GetObjectVisualizer()->ShowInput(true);
    m_ParentMainWindow->GetObjectVisualizer()->ShowObject(m_PhantomRegistration->GetStylusTipCoordinateFrame(), true);
    if (m_CurrentLandmarkIndex >= 3)
    {
      m_ParentMainWindow->GetObjectVisualizer()->ShowObject(m_PhantomRegistration->GetPhantomCoordinateFrame(), true);
    }

    ui.pushButton_RecordPoint->setFocus();
  }
  else if (m_State == ToolboxState_Done)
  {
    ui.label_Instructions->setText(QString("Registration error is %1 mm\nTransform is ready to save").arg(m_PhantomRegistration->GetRegistrationError(), 0, 'f', 6));

    ui.pushButton_OpenStylusCalibration->setEnabled(true);
    ui.pushButton_RecordPoint->setEnabled(false);
    ui.pushButton_Reset->setEnabled(true);
    ui.pushButton_Undo->setEnabled(true);

    m_ParentMainWindow->SetStatusBarText(QString(" Phantom registration done"));
    m_ParentMainWindow->SetStatusBarProgress(-1);

    m_ParentMainWindow->GetObjectVisualizer()->ShowInput(true);
    m_ParentMainWindow->GetObjectVisualizer()->ShowObject(m_PhantomRegistration->GetPhantomCoordinateFrame(), true);
    m_ParentMainWindow->GetObjectVisualizer()->ShowObject(m_PhantomRegistration->GetStylusTipCoordinateFrame(), true);

  }
  else if (m_State == ToolboxState_Error)
  {
    ui.label_StylusPosition->setText(tr("N/A"));
    ui.label_Instructions->setText("Error occured!");

    ui.pushButton_OpenStylusCalibration->setEnabled(true);
    ui.pushButton_RecordPoint->setEnabled(false);
    ui.pushButton_Reset->setEnabled(false);
    ui.pushButton_Undo->setEnabled(false);

    m_ParentMainWindow->SetStatusBarText(QString(""));
    m_ParentMainWindow->SetStatusBarProgress(-1);
  }
}

//-----------------------------------------------------------------------------

PlusStatus PhantomRegistrationToolbox::Start()
{
  LOG_TRACE("PhantomRegistrationToolbox::Start"); 

  // Check number of landmarks
  if (m_PhantomRegistration->GetDefinedLandmarks()->GetNumberOfPoints() < 4)
  {
    LOG_ERROR("Not enough (" << m_PhantomRegistration->GetDefinedLandmarks()->GetNumberOfPoints() << ") defined landmarks (should be at least 4)!");
    return PLUS_FAIL;
  }

  // Initialize toolbox canvas
  if (InitializeVisualization() != PLUS_SUCCESS)
  {
    LOG_ERROR("Initializing phantom registration visualization failed!");
    return PLUS_FAIL;
  }

  if (m_ParentMainWindow->GetObjectVisualizer()->IsExistingTransform(m_PhantomRegistration->GetStylusTipCoordinateFrame(), m_PhantomRegistration->GetReferenceCoordinateFrame()) == PLUS_SUCCESS)
  {
    m_CurrentLandmarkIndex = 0;

    // Initialize input points poly data in visualizer
    m_ParentMainWindow->GetObjectVisualizer()->GetInputPolyData()->GetPoints()->Initialize();
    m_ParentMainWindow->GetObjectVisualizer()->GetInputPolyData()->Modified();

    // Highlight first landmark
    m_RequestedLandmarkPolyData->GetPoints()->InsertPoint(0, m_PhantomRegistration->GetDefinedLandmarks()->GetPoint(0));
    m_RequestedLandmarkPolyData->GetPoints()->Modified();

    SetState(ToolboxState_InProgress);
  }
  else
  {
    LOG_ERROR("No stylus tip to reference transform available!");
    SetState(ToolboxState_Error);
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

void PhantomRegistrationToolbox::OpenStylusCalibration()
{
  LOG_TRACE("PhantomRegistrationToolbox::OpenStylusCalibration");

  // File open dialog for selecting phantom definition xml
  QString filter = QString( tr( "XML files ( *.xml );;" ) );
  QString fileName = QFileDialog::getOpenFileName(NULL, QString( tr( "Open stylus calibration XML" ) ), vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationDirectory(), filter);
  if (fileName.isNull())
  {
    return;
  }

  // Parse XML file
  vtkSmartPointer<vtkXMLDataElement> rootElement = vtkSmartPointer<vtkXMLDataElement>::Take(vtkXMLUtilities::ReadElementFromFile(fileName.toAscii().data()));
  if (rootElement == NULL)
  {  
    LOG_ERROR("Unable to read the configuration file: " << fileName.toAscii().data()); 
    return;
  }

  // Read stylus coordinate frame name
  vtkPivotCalibrationAlgo* pivotCalibrationAlgo = vtkPivotCalibrationAlgo::New();
  if (pivotCalibrationAlgo->ReadConfiguration( vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData() ) != PLUS_SUCCESS)
  {
    LOG_ERROR("Failed to read stylus coordinate frame name!");
    pivotCalibrationAlgo->Delete();
    return;
  }

  // Read stylus calibration transform
  PlusTransformName stylusTipToStylusTransformName(m_PhantomRegistration->GetStylusTipCoordinateFrame(), pivotCalibrationAlgo->GetObjectMarkerCoordinateFrame());
  vtkSmartPointer<vtkMatrix4x4> stylusTipToStylusTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  std::string transformDate;
  double transformError = 0.0;
  bool valid = false;
  vtkTransformRepository* tempTransformRepo = vtkTransformRepository::New();
  if ( tempTransformRepo->ReadConfiguration( vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData() ) != PLUS_SUCCESS
    || tempTransformRepo->GetTransform(stylusTipToStylusTransformName, stylusTipToStylusTransformMatrix, &valid) != PLUS_SUCCESS
    || tempTransformRepo->GetTransformDate(stylusTipToStylusTransformName, transformDate) != PLUS_SUCCESS
    || tempTransformRepo->GetTransformError(stylusTipToStylusTransformName, transformError) != PLUS_SUCCESS )
  {
    LOG_ERROR("Failed to read transform from opened file!");
    pivotCalibrationAlgo->Delete();
    tempTransformRepo->Delete();
    return;
  }

  tempTransformRepo->Delete();
  pivotCalibrationAlgo->Delete();

  if (valid)
  {
    if (m_ParentMainWindow->GetObjectVisualizer()->GetTransformRepository()->SetTransform(stylusTipToStylusTransformName, stylusTipToStylusTransformMatrix) != PLUS_SUCCESS)
    {
      LOG_ERROR("Failed to set stylus calibration transform to transform repository!");
      return;
    }

    m_ParentMainWindow->GetObjectVisualizer()->GetTransformRepository()->SetTransformDate(stylusTipToStylusTransformName, transformDate.c_str());
    m_ParentMainWindow->GetObjectVisualizer()->GetTransformRepository()->SetTransformError(stylusTipToStylusTransformName, transformError);
    m_ParentMainWindow->GetObjectVisualizer()->GetTransformRepository()->SetTransformPersistent(stylusTipToStylusTransformName, true);
  }
  else
  {
    LOG_ERROR("Invalid stylus calibration transform found, it was not set!");
  }

  // Set to InProgress if both stylus calibration and phantom definition are available
  Start();

  LOG_INFO("Stylus calibration imported in phantom registration toolbox from file '" << fileName.toAscii().data() << "'");
}

//-----------------------------------------------------------------------------

void PhantomRegistrationToolbox::RecordPoint()
{
  LOG_TRACE("PhantomRegistrationToolbox::RecordPoint"); 

  // If tracker is FakeTracker then set counter (trigger position change) and wait for it to apply the new position
  vtkDataCollectorHardwareDevice* dataCollectorHardwareDevice = dynamic_cast<vtkDataCollectorHardwareDevice*>(m_ParentMainWindow->GetObjectVisualizer()->GetDataCollector());
  if (dataCollectorHardwareDevice)
  {
    vtkFakeTracker *fakeTracker = dynamic_cast<vtkFakeTracker*>(dataCollectorHardwareDevice->GetTracker());
    if (fakeTracker != NULL)
    {
      fakeTracker->SetCounter(m_CurrentLandmarkIndex);
      fakeTracker->SetTransformRepository(m_ParentMainWindow->GetObjectVisualizer()->GetTransformRepository());
      vtkAccurateTimer::Delay(2.1 / fakeTracker->GetFrequency());
    }
  }

  // Acquire point
  vtkSmartPointer<vtkMatrix4x4> stylusTipToReferenceTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  bool valid = false;
  if (m_ParentMainWindow->GetObjectVisualizer()->GetTransformMatrix(m_PhantomRegistration->GetStylusTipCoordinateFrame(), m_PhantomRegistration->GetReferenceCoordinateFrame(), stylusTipToReferenceTransformMatrix, &valid) != PLUS_SUCCESS)
  {
    LOG_ERROR("No transform found between stylus and reference!");
    return;
  }

  // Add point to registration algorithm
  if (!valid)
  {
    LOG_WARNING("Invalid stylus tip to reference transform - cannot be added!");
    return;
  }

  double stylusTipPosition[4] = {stylusTipToReferenceTransformMatrix->GetElement(0,3), stylusTipToReferenceTransformMatrix->GetElement(1,3), stylusTipToReferenceTransformMatrix->GetElement(2,3), 1.0 };

  // Add recorded point to algorithm
  m_PhantomRegistration->GetRecordedLandmarks()->InsertPoint(m_CurrentLandmarkIndex, stylusTipPosition[0], stylusTipPosition[1], stylusTipPosition[2]);
  m_PhantomRegistration->GetRecordedLandmarks()->Modified();

  // Add recorded point to visualization
  m_ParentMainWindow->GetObjectVisualizer()->GetInputPolyData()->GetPoints()->InsertPoint(m_CurrentLandmarkIndex, stylusTipPosition[0], stylusTipPosition[1], stylusTipPosition[2]);
  m_ParentMainWindow->GetObjectVisualizer()->GetInputPolyData()->Modified();

  // Set new current landmark number and reset request flag
  ++m_CurrentLandmarkIndex;

  LOG_INFO("Point recorded for phantom registration");

  // If there are at least 3 acuired points then register
  if (m_CurrentLandmarkIndex >= 3)
  {
    if (m_PhantomRegistration->Register( m_ParentMainWindow->GetObjectVisualizer()->GetTransformRepository() ) == PLUS_SUCCESS)
    {
      m_ParentMainWindow->GetObjectVisualizer()->ShowObject(m_PhantomRegistration->GetPhantomCoordinateFrame(), true);
    }
    else
    {
      LOG_ERROR("Phantom registration failed!");
    }
  }

  // If it was the last landmark then write configuration, set status to done and reset landmark counter
  if (m_CurrentLandmarkIndex == m_PhantomRegistration->GetDefinedLandmarks()->GetNumberOfPoints())
  {
    if (m_ParentMainWindow->GetObjectVisualizer()->GetTransformRepository()->WriteConfiguration( vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData() ) != PLUS_SUCCESS)
    {
      LOG_ERROR("Unable to save phantom registration result in configuration XML tree!");
      SetState(ToolboxState_Error);
      return;
    }

    SetState(ToolboxState_Done);

    m_RequestedLandmarkPolyData->GetPoints()->GetData()->RemoveTuple(0);
    m_RequestedLandmarkPolyData->GetPoints()->Modified();

    LOG_INFO("Phantom registration performed successfully");
  }
  else
  {
    // Highlight next landmark
    m_RequestedLandmarkPolyData->GetPoints()->InsertPoint(0, m_PhantomRegistration->GetDefinedLandmarks()->GetPoint(m_CurrentLandmarkIndex));
    m_RequestedLandmarkPolyData->GetPoints()->Modified();
  }

  // Reset camera after each recording
  m_ParentMainWindow->GetObjectVisualizer()->GetCanvasRenderer()->ResetCamera();
}

//-----------------------------------------------------------------------------

void PhantomRegistrationToolbox::Undo()
{
  LOG_TRACE("PhantomRegistrationToolbox::Undo"); 

  if (m_State == ToolboxState_Done) {
    SetState(ToolboxState_InProgress);
  }

  if (m_CurrentLandmarkIndex > 0)
  {
    // Decrease current landmark index
    --m_CurrentLandmarkIndex;

    // Reset result transform (in case Undo was pressed when the registration was ready)
    m_PhantomRegistration->SetPhantomToReferenceTransformMatrix(NULL);

    // Delete previously acquired landmark
    m_ParentMainWindow->GetObjectVisualizer()->GetInputPolyData()->GetPoints()->GetData()->RemoveTuple(m_CurrentLandmarkIndex);
    m_ParentMainWindow->GetObjectVisualizer()->GetInputPolyData()->Modified();

    // Highlight previous landmark
    m_RequestedLandmarkPolyData->GetPoints()->InsertPoint(0, m_PhantomRegistration->GetDefinedLandmarks()->GetPoint(m_CurrentLandmarkIndex));
    m_RequestedLandmarkPolyData->GetPoints()->Modified();

    // Hide phantom from main canvas
    m_ParentMainWindow->GetObjectVisualizer()->ShowObject(m_PhantomRegistration->GetReferenceCoordinateFrame(), false);
  }

  // If tracker is FakeTracker then set counter
  vtkDataCollectorHardwareDevice* dataCollectorHardwareDevice = dynamic_cast<vtkDataCollectorHardwareDevice*>(m_ParentMainWindow->GetObjectVisualizer()->GetDataCollector());
  if (dataCollectorHardwareDevice)
  {
    vtkFakeTracker *fakeTracker = dynamic_cast<vtkFakeTracker*>(dataCollectorHardwareDevice->GetTracker());
    if (fakeTracker != NULL)
    {
      fakeTracker->SetCounter(m_CurrentLandmarkIndex);
    }
  }

  LOG_INFO("Undo last step of phantom registration");
}

//-----------------------------------------------------------------------------

void PhantomRegistrationToolbox::Reset()
{
  LOG_TRACE("PhantomRegistrationToolbox::Reset"); 

  if (m_State == ToolboxState_Done)
  {
    SetState(ToolboxState_InProgress);
  }

  // Delete acquired landmarks
  vtkSmartPointer<vtkPoints> landmarkPoints = vtkSmartPointer<vtkPoints>::New();
  m_ParentMainWindow->GetObjectVisualizer()->GetInputPolyData()->SetPoints(landmarkPoints);
  m_ParentMainWindow->GetObjectVisualizer()->GetInputPolyData()->Modified();

  // Reset current landmark index
  m_CurrentLandmarkIndex = 0;

  // Reset result transform (if Reset was pressed when the registration was ready we have to make it null)
  m_PhantomRegistration->SetPhantomToReferenceTransformMatrix(NULL);

  // Highlight first landmark
  if ((m_State != ToolboxState_Uninitialized) && (m_PhantomRegistration->GetDefinedLandmarks()->GetNumberOfPoints() > 0))
  {
    m_RequestedLandmarkPolyData->GetPoints()->InsertPoint(0, m_PhantomRegistration->GetDefinedLandmarks()->GetPoint(0));
    m_RequestedLandmarkPolyData->GetPoints()->Modified();
  }

  // Hide phantom from main canvas
  m_ParentMainWindow->GetObjectVisualizer()->ShowObject(m_PhantomRegistration->GetReferenceCoordinateFrame(), false);

  // If tracker is FakeTracker then reset counter
  vtkDataCollectorHardwareDevice* dataCollectorHardwareDevice = dynamic_cast<vtkDataCollectorHardwareDevice*>(m_ParentMainWindow->GetObjectVisualizer()->GetDataCollector());
  if (dataCollectorHardwareDevice)
  {
    vtkFakeTracker *fakeTracker = dynamic_cast<vtkFakeTracker*>(dataCollectorHardwareDevice->GetTracker());
    if (fakeTracker != NULL)
    {
      fakeTracker->SetCounter(m_CurrentLandmarkIndex);
    }
  }

  LOG_INFO("Reset phantom registration");
}