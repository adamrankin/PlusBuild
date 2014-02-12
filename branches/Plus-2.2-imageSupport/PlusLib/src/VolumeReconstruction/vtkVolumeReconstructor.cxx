/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#include "PlusConfigure.h"

#include "vtkVolumeReconstructor.h"

#include <limits>

#include "vtkImageImport.h" 
#include "vtkImageData.h" 
#include "vtkImageViewer.h"
#include "vtkObjectFactory.h"
#include "vtkSmartPointer.h"
#include "vtkTransform.h"
#include "vtkXMLUtilities.h"
#include "vtkImageExtractComponents.h"
#include "vtkDataSetWriter.h"

#include "vtkPasteSliceIntoVolume.h"
#include "vtkFillHolesInVolume.h"
#include "vtkTrackedFrameList.h"
#include "TrackedFrame.h"
#include "vtkTransformRepository.h"

#include "metaImage.h"

vtkCxxRevisionMacro(vtkVolumeReconstructor, "$Revisions: 1.0 $");
vtkStandardNewMacro(vtkVolumeReconstructor);

//----------------------------------------------------------------------------
vtkVolumeReconstructor::vtkVolumeReconstructor()
: ImageCoordinateFrame(NULL)
, ReferenceCoordinateFrame(NULL)
{
  this->ReconstructedVolume = vtkSmartPointer<vtkImageData>::New();
  this->Reconstructor = vtkPasteSliceIntoVolume::New();  
  this->HoleFiller = vtkFillHolesInVolume::New();  
  this->FillHoles = 0;
  this->SkipInterval = 1;
  this->ReconstructedVolumeUpdatedTime = 0;
}

//----------------------------------------------------------------------------
vtkVolumeReconstructor::~vtkVolumeReconstructor()
{
  if (this->Reconstructor)
  {
    this->Reconstructor->Delete();
    this->Reconstructor=NULL;
  }

  if (this->HoleFiller)
  {
    this->HoleFiller->Delete();
    this->HoleFiller=NULL;
  }
  SetImageCoordinateFrame(NULL);
  SetReferenceCoordinateFrame(NULL);
}

//----------------------------------------------------------------------------
void vtkVolumeReconstructor::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::ReadConfiguration(vtkXMLDataElement* config)
{
  if (config==NULL)
  {
    LOG_ERROR("vtkVolumeReconstructor::ReadConfiguration failed: config root element is NULL");
    return PLUS_FAIL;
  }
  vtkXMLDataElement* reconConfig = config->FindNestedElementWithName("VolumeReconstruction");
  if (reconConfig == NULL)
  {
    LOG_ERROR("vtkVolumeReconstructor::ReadConfiguration failed: No volume reconstruction is found in the XML tree!");
    return PLUS_FAIL;
  }

  // Reference coordinate system: where the volume will be reconstructed in
  const char* referenceCoordinateFrame=reconConfig->GetAttribute("ReferenceCoordinateFrame");
  if (referenceCoordinateFrame!=NULL)
  {
    SetReferenceCoordinateFrame(referenceCoordinateFrame);
  }
  // Image coordinate system: name of the 2D image frame coordinate system
  const char* imageCoordinateFrame=reconConfig->GetAttribute("ImageCoordinateFrame");
  if (imageCoordinateFrame!=NULL)
  {
    SetImageCoordinateFrame(imageCoordinateFrame);
  }

  // output volume parameters
  // origin and spacing is defined in the reference coordinate system
  double outputSpacing[3]={0,0,0};
  if (reconConfig->GetVectorAttribute("OutputSpacing", 3, outputSpacing))
  {
    this->Reconstructor->SetOutputSpacing(outputSpacing);
  }
  else
  {
    LOG_ERROR("OutputSpacing parameter is not found!");
    return PLUS_FAIL;
  }
  double outputOrigin[3]={0,0,0};
  if (reconConfig->GetVectorAttribute("OutputOrigin", 3, outputOrigin))
  {
    this->Reconstructor->SetOutputOrigin(outputOrigin);
  }
  int outputExtent[6]={0,0,0,0,0,0};
  if (reconConfig->GetVectorAttribute("OutputExtent", 6, outputExtent))
  {
    this->Reconstructor->SetOutputExtent(outputExtent);
  }

  // clipping parameters
  int clipRectangleOrigin[2]={0,0};
  if (reconConfig->GetVectorAttribute("ClipRectangleOrigin", 2, clipRectangleOrigin))
  {
    this->Reconstructor->SetClipRectangleOrigin(clipRectangleOrigin);
  }
  int clipRectangleSize[2]={0,0};
  if (reconConfig->GetVectorAttribute("ClipRectangleSize", 2, clipRectangleSize))
  {
    this->Reconstructor->SetClipRectangleSize(clipRectangleSize);
  }

  // fan parameters
  double fanAngles[2]={0,0};
  if (reconConfig->GetVectorAttribute("FanAngles", 2, fanAngles))
  {
    this->Reconstructor->SetFanAngles(fanAngles);
  }
  double fanOrigin[2]={0,0};
  if (reconConfig->GetVectorAttribute("FanOrigin", 2, fanOrigin))
  {
    this->Reconstructor->SetFanOrigin(fanOrigin);
  }
  double fanDepth=0;
  if (reconConfig->GetScalarAttribute("FanDepth", fanDepth))
  {
    this->Reconstructor->SetFanDepth(fanDepth);
  }

  if (reconConfig->GetScalarAttribute("SkipInterval",SkipInterval))
  {
    if (SkipInterval < 1)
    {
      LOG_WARNING("SkipInterval in the config file must be greater or equal to 1. Resetting to 1");
      SkipInterval = 1;
    }
  }

  // reconstruction options
  if (reconConfig->GetAttribute("Interpolation"))
  {
    if (STRCASECMP(reconConfig->GetAttribute("Interpolation"),
      this->Reconstructor->GetInterpolationModeAsString(vtkPasteSliceIntoVolume::LINEAR_INTERPOLATION)) == 0)
    {
      this->Reconstructor->SetInterpolationMode(vtkPasteSliceIntoVolume::LINEAR_INTERPOLATION);
    }
    else if (STRCASECMP(reconConfig->GetAttribute("Interpolation"), 
      this->Reconstructor->GetInterpolationModeAsString(vtkPasteSliceIntoVolume::NEAREST_NEIGHBOR_INTERPOLATION)) == 0)
    {
      this->Reconstructor->SetInterpolationMode(vtkPasteSliceIntoVolume::NEAREST_NEIGHBOR_INTERPOLATION);
    }
    else
    {
      LOG_ERROR("Unknown interpolation option: "<<reconConfig->GetAttribute("Interpolation")<<". Valid options: LINEAR, NEAREST_NEIGHBOR.");
    }
  }
  if (reconConfig->GetAttribute("Calculation"))
  {
    if (STRCASECMP(reconConfig->GetAttribute("Calculation"),
      this->Reconstructor->GetCalculationModeAsString(vtkPasteSliceIntoVolume::WEIGHTED_AVERAGE)) == 0)
    {
      this->Reconstructor->SetCalculationMode(vtkPasteSliceIntoVolume::WEIGHTED_AVERAGE);
    }
    else if (STRCASECMP(reconConfig->GetAttribute("Calculation"), 
      this->Reconstructor->GetCalculationModeAsString(vtkPasteSliceIntoVolume::MAXIMUM)) == 0)
    {
      this->Reconstructor->SetCalculationMode(vtkPasteSliceIntoVolume::MAXIMUM);
    }
    else
    {
      LOG_ERROR("Unknown calculation option: "<<reconConfig->GetAttribute("Calculation")<<". Valid options: WEIGHTED_AVERAGE, MAXIMUM.");
    }
  }
  if (reconConfig->GetAttribute("Optimization"))
  {
    if (STRCASECMP(reconConfig->GetAttribute("Optimization"), 
      this->Reconstructor->GetOptimizationModeAsString(vtkPasteSliceIntoVolume::FULL_OPTIMIZATION)) == 0)
    {
      this->Reconstructor->SetOptimization(vtkPasteSliceIntoVolume::FULL_OPTIMIZATION);
    }
    else if (STRCASECMP(reconConfig->GetAttribute("Optimization"), 
      this->Reconstructor->GetOptimizationModeAsString(vtkPasteSliceIntoVolume::PARTIAL_OPTIMIZATION)) == 0)
    {
      this->Reconstructor->SetOptimization(vtkPasteSliceIntoVolume::PARTIAL_OPTIMIZATION);
    }
    else if (STRCASECMP(reconConfig->GetAttribute("Optimization"), 
      this->Reconstructor->GetOptimizationModeAsString(vtkPasteSliceIntoVolume::NO_OPTIMIZATION)) == 0)
    {
      this->Reconstructor->SetOptimization(vtkPasteSliceIntoVolume::NO_OPTIMIZATION);
    }
    else
    {
      LOG_ERROR("Unknown optimization option: "<<reconConfig->GetAttribute("Optimization")<<". Valid options: FULL, PARTIAL, NONE.");
    }
  }
  if (reconConfig->GetAttribute("Compounding"))
  {
    ((STRCASECMP(reconConfig->GetAttribute("Compounding"), "On") == 0) ? 
      this->Reconstructor->SetCompounding(1) : this->Reconstructor->SetCompounding(0));
  }

  int numberOfThreads=0;
  if (reconConfig->GetScalarAttribute("NumberOfThreads", numberOfThreads))
  {
    this->Reconstructor->SetNumberOfThreads(numberOfThreads);
    this->HoleFiller->SetNumberOfThreads(numberOfThreads);
  }

  int fillHoles=0;
  if (reconConfig->GetAttribute("FillHoles"))
  {
    if (STRCASECMP(reconConfig->GetAttribute("FillHoles"), "On") == 0) 
    {
      this->FillHoles=1;
    }
    else
    {
      this->FillHoles=0;
    }
  }

  // Find and read kernels. First for loop counts the number of kernels to allocate, second for loop stores them
  if (this->FillHoles) 
  {
    // load input for kernel size, stdev, etc...
    vtkXMLDataElement* holeFilling = reconConfig->FindNestedElementWithName("HoleFilling");
    if ( this->FillHoles == 1 && holeFilling == NULL )
    {
      LOG_ERROR("Couldn't locate hole filling parameters for hole filling!");
      return PLUS_FAIL;
    }

    // find the number of kernels
    int numHFElements(0);
    for ( int nestedElementIndex = 0; nestedElementIndex < holeFilling->GetNumberOfNestedElements(); ++nestedElementIndex )
    {
      vtkXMLDataElement* nestedElement = holeFilling->GetNestedElement(nestedElementIndex);
      if ( STRCASECMP(nestedElement->GetName(), "HoleFillingElement" ) != 0 )
      {
        // Not a kernel element, skip it
        continue; 
      }
      numHFElements++;
    } 

    // read each kernel into memory
    HoleFiller->SetNumHFElements(numHFElements);
    HoleFiller->AllocateHFElements();
    int numberOfErrors(0);
    int currentElementIndex(0);

    for ( int nestedElementIndex = 0; nestedElementIndex < holeFilling->GetNumberOfNestedElements(); ++nestedElementIndex )
    {
      vtkXMLDataElement* nestedElement = holeFilling->GetNestedElement(nestedElementIndex);
      if ( STRCASECMP(nestedElement->GetName(), "HoleFillingElement" ) != 0 )
      {
        // Not a hole filling element, skip it
        continue; 
      }

      FillHolesInVolumeElement tempElement;

      if (nestedElement->GetAttribute("Type"))
      {
        if (STRCASECMP(nestedElement->GetAttribute("Type"), "GAUSSIAN") == 0)
        {
          tempElement.type = FillHolesInVolumeElement::HFTYPE_GAUSSIAN;
        }
        else if (STRCASECMP(nestedElement->GetAttribute("Type"), "GAUSSIAN_ACCUMULATION") == 0)
        {
          tempElement.type = FillHolesInVolumeElement::HFTYPE_GAUSSIAN_ACCUMULATION;
        }
        else if (STRCASECMP(nestedElement->GetAttribute("Type"), "STICK") == 0)
        {
          tempElement.type = FillHolesInVolumeElement::HFTYPE_STICK;
        }
        else if (STRCASECMP(nestedElement->GetAttribute("Type"), "NEAREST_NEIGHBOR") == 0)
        {
          tempElement.type = FillHolesInVolumeElement::HFTYPE_NEAREST_NEIGHBOR;
        }
        else if (STRCASECMP(nestedElement->GetAttribute("Type"), "DISTANCE_WEIGHT_INVERSE") == 0)
        {
          tempElement.type = FillHolesInVolumeElement::HFTYPE_DISTANCE_WEIGHT_INVERSE;
        }
        else
        {
          LOG_ERROR("Unknown hole filling element option: "<<nestedElement->GetAttribute("Type")<<". Valid options: GAUSSIAN, STICK.");
        }
      }
      else
      {
        LOG_ERROR("Couldn't identify the hole filling element \"Type\"! Valid options: GAUSSIAN, STICK.");
        return PLUS_FAIL;
      }

      int size=0; 
      float stdev=0; 
      float minRatio = 0.; 
      int stickLengthLimit = 0;
      int numberOfSticksToUse = 1;

      switch (tempElement.type)
      {
      case FillHolesInVolumeElement::HFTYPE_GAUSSIAN:
      case FillHolesInVolumeElement::HFTYPE_GAUSSIAN_ACCUMULATION:
        // read stdev
        if ( nestedElement->GetScalarAttribute("Stdev", stdev) )
        {
          tempElement.stdev = stdev;
        }
        else
        {
          LOG_ERROR("Unable to find \"Stdev\" attribute of kernel[" << nestedElementIndex <<"]"); 
          numberOfErrors++; 
          continue; 
        }
      // note that at this point, GAUSSIAN and GAUSSIAN_ACCUMULATION will also read what is below (there is no break). This is intended.
      case FillHolesInVolumeElement::HFTYPE_DISTANCE_WEIGHT_INVERSE:
      case FillHolesInVolumeElement::HFTYPE_NEAREST_NEIGHBOR:
        // read size
        if ( nestedElement->GetScalarAttribute("Size", size) )
        {
          tempElement.size = size;
        }
        else
        {
          LOG_ERROR("Unable to find \"Size\" attribute of kernel[" << nestedElementIndex <<"]"); 
          numberOfErrors++; 
          continue; 
        }
        // read minRatio
        if ( nestedElement->GetScalarAttribute("MinimumKnownVoxelsRatio", minRatio) )
        {
          tempElement.minRatio = minRatio; 
        }
        else
        {
          LOG_ERROR("Unable to find \"MinimumKnownVoxelsRatio\" attribute of kernel[" << nestedElementIndex <<"]"); 
          numberOfErrors++; 
          continue; 
        }
        break;
      case FillHolesInVolumeElement::HFTYPE_STICK:
        // read stick
        if ( nestedElement->GetScalarAttribute("StickLengthLimit", stickLengthLimit) )
        {
          tempElement.stickLengthLimit = stickLengthLimit; 
        }
        else
        {
          LOG_ERROR("Unable to find \"StickLengthLimit\" attribute of hole filling element[" << nestedElementIndex <<"]"); 
          numberOfErrors++; 
          continue; 
        }
        if ( nestedElement->GetScalarAttribute("NumberOfSticksToUse", numberOfSticksToUse) )
        {
          tempElement.numSticksToUse = numberOfSticksToUse; 
        }
        else
        {
          LOG_ERROR("Unable to find \"NumberOfSticksToUse\" attribute of hole filling element[" << nestedElementIndex <<"]"); 
          numberOfErrors++; 
          continue; 
        }
        break;
      }

      HoleFiller->SetHFElement(currentElementIndex,tempElement);
      currentElementIndex++;
    }

    if (numberOfErrors != 0)
    {
      return PLUS_FAIL;
    }
  }

  this->Modified();

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
// Get the XML element describing the freehand object
PlusStatus vtkVolumeReconstructor::WriteConfiguration(vtkXMLDataElement *config)
{
  if ( config == NULL )
  {
    LOG_ERROR("Unable to write configuration from volume reconstructor! (XML data element is NULL)"); 
    return PLUS_FAIL; 
  }

  vtkXMLDataElement* reconConfig = config->FindNestedElementWithName("VolumeReconstruction");
  if (reconConfig == NULL)
  {
    vtkSmartPointer<vtkXMLDataElement> newReconConfig = vtkSmartPointer<vtkXMLDataElement>::New();
    newReconConfig->SetName("VolumeReconstruction");
    config->AddNestedElement(newReconConfig);
    reconConfig = config->FindNestedElementWithName("VolumeReconstruction");
    if (reconConfig == NULL)
    {
      LOG_ERROR("Failed to add VolumeReconstruction element");
      return PLUS_FAIL;
    }
  }

  reconConfig->SetAttribute("ImageCoordinateFrame", this->ImageCoordinateFrame);
  reconConfig->SetAttribute("ReferenceCoordinateFrame", this->ReferenceCoordinateFrame);

  // output parameters
  reconConfig->SetVectorAttribute("OutputSpacing", 3, this->Reconstructor->GetOutputSpacing());
  reconConfig->SetVectorAttribute("OutputOrigin", 3, this->Reconstructor->GetOutputOrigin());
  reconConfig->SetVectorAttribute("OutputExtent", 6, this->Reconstructor->GetOutputExtent());

  // clipping parameters
  reconConfig->SetVectorAttribute("ClipRectangleOrigin", 2, this->Reconstructor->GetClipRectangleOrigin());
  reconConfig->SetVectorAttribute("ClipRectangleSize", 2, this->Reconstructor->GetClipRectangleSize());

  // fan parameters
  if (this->Reconstructor->FanClippingApplied())
  {
    reconConfig->SetVectorAttribute("FanAngles", 2, this->Reconstructor->GetFanAngles());
    reconConfig->SetVectorAttribute("FanOrigin", 2, this->Reconstructor->GetFanOrigin());
    reconConfig->SetDoubleAttribute("FanDepth", this->Reconstructor->GetFanDepth());
  }
  else
  {
    reconConfig->RemoveAttribute("FanAngles");
    reconConfig->RemoveAttribute("FanOrigin");
    reconConfig->RemoveAttribute("FanDepth");
  }

  // reconstruction options
  reconConfig->SetAttribute("Interpolation", this->Reconstructor->GetInterpolationModeAsString(this->Reconstructor->GetInterpolationMode()));
  reconConfig->SetAttribute("Optimization", this->Reconstructor->GetOptimizationModeAsString(this->Reconstructor->GetOptimization()));
  reconConfig->SetAttribute("Compounding", this->Reconstructor->GetCompounding()?"On":"Off");

  if (this->Reconstructor->GetNumberOfThreads()>0)
  {
    reconConfig->SetIntAttribute("NumberOfThreads", this->Reconstructor->GetNumberOfThreads());
  }
  else
  {
    reconConfig->RemoveAttribute("NumberOfThreads");
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
void vtkVolumeReconstructor::AddImageToExtent( vtkImageData *image, vtkMatrix4x4* imageToReference, double* extent_Ref)
{
  // Output volume is in the Reference coordinate system.

  // Prepare the four corner points of the input US image.
  int* frameExtent=image->GetExtent();
  std::vector< double* > corners_ImagePix;
  double minX=frameExtent[0];
  double maxX=frameExtent[1];
  double minY=frameExtent[2];
  double maxY=frameExtent[3];
  if (this->Reconstructor->GetClipRectangleSize()[0]>0 && this->Reconstructor->GetClipRectangleSize()[1]>0)
  {
    // Clipping rectangle is specified
    minX=std::max<double>(minX, this->Reconstructor->GetClipRectangleOrigin()[0]);
    maxX=std::min<double>(maxX, this->Reconstructor->GetClipRectangleOrigin()[0]+this->Reconstructor->GetClipRectangleSize()[0]);
    minY=std::max<double>(minY, this->Reconstructor->GetClipRectangleOrigin()[1]);
    maxY=std::min<double>(maxY, this->Reconstructor->GetClipRectangleOrigin()[1]+this->Reconstructor->GetClipRectangleSize()[1]);
  }
  double c0[ 4 ] = { minX, minY, 0,  1 };
  double c1[ 4 ] = { minX, maxY, 0,  1 };
  double c2[ 4 ] = { maxX, minY, 0,  1 };
  double c3[ 4 ] = { maxX, maxY, 0,  1 };
  corners_ImagePix.push_back( c0 );
  corners_ImagePix.push_back( c1 );
  corners_ImagePix.push_back( c2 );
  corners_ImagePix.push_back( c3 );

  // Transform the corners to Reference and expand the extent if needed
  for ( unsigned int corner = 0; corner < corners_ImagePix.size(); ++corner )
  {
    double corner_Ref[ 4 ] = { 0, 0, 0, 1 }; // position of the corner in the Reference coordinate system
    imageToReference->MultiplyPoint( corners_ImagePix[corner], corner_Ref );

    for ( int axis = 0; axis < 3; axis ++ )
    {
      if ( corner_Ref[axis] < extent_Ref[axis*2] )
      {
        // min extent along this coord axis has to be decreased
        extent_Ref[axis*2]=corner_Ref[axis];
      }
      if ( corner_Ref[axis] > extent_Ref[axis*2+1] )
      {
        // max extent along this coord axis has to be increased
        extent_Ref[axis*2+1]=corner_Ref[axis];
      }
    }
  }
} 

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::GetImageToReferenceTransformName(PlusTransformName& imageToReferenceTransformName)
{
  if (this->ReferenceCoordinateFrame!=NULL && this->ImageCoordinateFrame!=NULL)
  {
    // image to reference transform is specified in the XML tree
    imageToReferenceTransformName=PlusTransformName(this->ImageCoordinateFrame,this->ReferenceCoordinateFrame);
    if (!imageToReferenceTransformName.IsValid())
    { 
      LOG_ERROR("Failed to set ImageToReference transform name from '" << this->ImageCoordinateFrame <<"' to '"<<this->ReferenceCoordinateFrame<<"'" ); 
      return PLUS_FAIL;
    }
    return PLUS_SUCCESS;
  }
  if (this->ImageCoordinateFrame==NULL)
  {
    LOG_ERROR("Image coordinate frame name is undefined");
  }
  if (this->ReferenceCoordinateFrame==NULL)
  {
    LOG_ERROR("Reference coordinate frame name is undefined");
  }
  return PLUS_FAIL;
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::SetOutputExtentFromFrameList(vtkTrackedFrameList* trackedFrameList, vtkTransformRepository* transformRepository)
{
  PlusTransformName imageToReferenceTransformName;
  if (GetImageToReferenceTransformName(imageToReferenceTransformName)!=PLUS_SUCCESS)
  {
    LOG_ERROR("Invalid ImageToReference transform name"); 
    return PLUS_FAIL; 
  }

  if ( trackedFrameList == NULL )
  {
    LOG_ERROR("Failed to set output extent from tracked frame list - input frame list is NULL!"); 
    return PLUS_FAIL; 
  }
  if ( trackedFrameList->GetNumberOfTrackedFrames() == 0)
  {
    LOG_ERROR("Failed to set output extent from tracked frame list - input frame list is empty!"); 
    return PLUS_FAIL; 
  }

  if ( transformRepository == NULL )
  {
    LOG_ERROR("Failed to set output extent from tracked frame list - input transform repository is NULL!"); 
    return PLUS_FAIL; 
  }

  double extent_Ref[6]=
  {
    VTK_DOUBLE_MAX, VTK_DOUBLE_MIN,
    VTK_DOUBLE_MAX, VTK_DOUBLE_MIN,
    VTK_DOUBLE_MAX, VTK_DOUBLE_MIN
  };

  const int numberOfFrames = trackedFrameList->GetNumberOfTrackedFrames();
  int numberOfValidFrames = 0;
  for (int frameIndex = 0; frameIndex < numberOfFrames; ++frameIndex )
  {
    TrackedFrame* frame = trackedFrameList->GetTrackedFrame( frameIndex );
    
    if ( transformRepository->SetTransforms(*frame) != PLUS_SUCCESS )
    {
      LOG_ERROR("Failed to update transform repository with tracked frame!"); 
      return PLUS_FAIL; 
    }

    // Get transform
    bool isMatrixValid(false); 
    vtkSmartPointer<vtkMatrix4x4> imageToReferenceTransformMatrix=vtkSmartPointer<vtkMatrix4x4>::New();
    if ( transformRepository->GetTransform(imageToReferenceTransformName, imageToReferenceTransformMatrix, &isMatrixValid ) != PLUS_SUCCESS )
    {
      std::string strImageToReferenceTransformName; 
      imageToReferenceTransformName.GetTransformName(strImageToReferenceTransformName); 
      LOG_ERROR("Failed to get transform '"<<strImageToReferenceTransformName<<"' from transform repository!"); 
      return PLUS_FAIL; 
    }

    if ( isMatrixValid )
    {
      numberOfValidFrames++;

      // Get image (only the frame extents will be used)
      vtkImageData* frameImage=trackedFrameList->GetTrackedFrame(frameIndex)->GetImageData()->GetImage();

      // Expand the extent_Ref to include this frame
      AddImageToExtent(frameImage, imageToReferenceTransformMatrix, extent_Ref);
    }
  }

  LOG_DEBUG("Automatic volume extent computation from frames used "<<numberOfValidFrames<<" out of "<<numberOfFrames<<" (probably wrong image or reference coordinate system was defined or all transforms were invalid)");
  if (numberOfValidFrames==0)
  {
    std::string strImageToReferenceTransformName; 
    imageToReferenceTransformName.GetTransformName(strImageToReferenceTransformName);
    LOG_ERROR("Automatic volume extent computation failed, there were no valid "<<strImageToReferenceTransformName<<" transform available in the whole sequence");
    return PLUS_FAIL;
  }

  // Set the output extent from the current min and max values, using the user-defined image resolution.
  int outputExtent[ 6 ] = { 0, 0, 0, 0, 0, 0 };
  double* outputSpacing = this->Reconstructor->GetOutputSpacing();
  outputExtent[ 1 ] = int( ( extent_Ref[1] - extent_Ref[0] ) / outputSpacing[ 0 ] );
  outputExtent[ 3 ] = int( ( extent_Ref[3] - extent_Ref[2] ) / outputSpacing[ 1 ] );
  outputExtent[ 5 ] = int( ( extent_Ref[5] - extent_Ref[4] ) / outputSpacing[ 2 ] );

  this->Reconstructor->SetOutputScalarMode(trackedFrameList->GetTrackedFrame(0)->GetImageData()->GetImage()->GetScalarType());
  this->Reconstructor->SetOutputExtent( outputExtent );
  this->Reconstructor->SetOutputOrigin( extent_Ref[0], extent_Ref[2], extent_Ref[4] ); 
  try
  {
    if (this->Reconstructor->ResetOutput()!=PLUS_SUCCESS) // :TODO: call this automatically
    {
      LOG_ERROR("Failed to initialize output of the reconstructor");
      return PLUS_FAIL;
    }
  }
  catch(std::bad_alloc& e)
  {
    cerr << e.what() << endl;
    LOG_ERROR("StartReconstruction failed due to out of memory. Try to reduce the size or increase spacing of the output volume.");
    return PLUS_FAIL;
  }

  this->Modified();

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::AddTrackedFrame(TrackedFrame* frame, vtkTransformRepository* transformRepository, bool* insertedIntoVolume/*=NULL*/)
{
  PlusTransformName imageToReferenceTransformName;
  if (GetImageToReferenceTransformName(imageToReferenceTransformName)!=PLUS_SUCCESS)
  {
    LOG_ERROR("Invalid ImageToReference transform name"); 
    return PLUS_FAIL; 
  }

  if ( frame == NULL )
  {
    LOG_ERROR("Failed to add tracked frame to volume - input frame is NULL"); 
    return PLUS_FAIL; 
  }

  if ( transformRepository == NULL )
  {
    LOG_ERROR("Failed to add tracked frame to volume - input transform repository is NULL"); 
    return PLUS_FAIL; 
  }

  bool isMatrixValid(false); 
  vtkSmartPointer<vtkMatrix4x4> imageToReferenceTransformMatrix=vtkSmartPointer<vtkMatrix4x4>::New();
  if ( transformRepository->GetTransform(imageToReferenceTransformName, imageToReferenceTransformMatrix, &isMatrixValid ) != PLUS_SUCCESS )
  {
    std::string strImageToReferenceTransformName; 
    imageToReferenceTransformName.GetTransformName(strImageToReferenceTransformName); 
    LOG_ERROR("Failed to get transform '"<<strImageToReferenceTransformName<<"' from transform repository"); 
    return PLUS_FAIL; 
  }

  if ( insertedIntoVolume != NULL )
  {
    *insertedIntoVolume = isMatrixValid; 
  }

  if ( !isMatrixValid )
  {
    // Insert only valid frame into volume
    std::string strImageToReferenceTransformName; 
    imageToReferenceTransformName.GetTransformName(strImageToReferenceTransformName); 
    LOG_DEBUG("Transform '"<<strImageToReferenceTransformName<<"' is invalid for the current frame, therefore this frame is not be inserted into the volume"); 
    return PLUS_SUCCESS; 
  }

  vtkImageData* frameImage=frame->GetImageData()->GetImage();

  this->Modified();

  return this->Reconstructor->InsertSlice(frameImage, imageToReferenceTransformMatrix);
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::UpdateReconstructedVolume()
{
  // Load reconstructed volume if the algorithm configuration was modified since the last load
  //   MTime is updated whenever a new frame is added or configuration modified
  //   ReconstructedVolumeUpdatedTime is updated whenever a reconstruction was completed
  if (this->ReconstructedVolumeUpdatedTime >= this->GetMTime())
  {
    // reconstruction is already up-to-date
    return PLUS_SUCCESS;
  }

  if (this->FillHoles)
  {
    if (this->GenerateHoleFilledVolume() != PLUS_SUCCESS)
    {
      LOG_ERROR("Failed to generate hole filled volume!");
      return PLUS_FAIL;
    }
  }
  else
  {
    this->ReconstructedVolume->DeepCopy(this->Reconstructor->GetReconstructedVolume());
  }

  this->ReconstructedVolumeUpdatedTime = this->GetMTime();

  return PLUS_SUCCESS; 
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::GetReconstructedVolume(vtkImageData* volume)
{
  if (this->UpdateReconstructedVolume() != PLUS_SUCCESS)
  {
    LOG_ERROR("Failed to load reconstructed volume");
    return PLUS_FAIL;
  }

  volume->DeepCopy(this->ReconstructedVolume);

  return PLUS_SUCCESS; 
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::GenerateHoleFilledVolume()
{
  LOG_INFO("Hole Filling has begun");
  this->HoleFiller->SetReconstructedVolume(this->Reconstructor->GetReconstructedVolume());
  this->HoleFiller->SetAccumulationBuffer(this->Reconstructor->GetAccumulationBuffer());
  this->HoleFiller->Update();
  LOG_INFO("Hole Filling has finished");

  this->ReconstructedVolume->DeepCopy(HoleFiller->GetOutput());

  return PLUS_SUCCESS; 
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::ExtractGrayLevels(vtkImageData* reconstructedVolume)
{  
  if (this->UpdateReconstructedVolume() != PLUS_SUCCESS)
  {
    LOG_ERROR("Failed to load reconstructed volume");
    return PLUS_FAIL;
  }

  vtkSmartPointer<vtkImageExtractComponents> extract = vtkSmartPointer<vtkImageExtractComponents>::New();          

  // Keep only first component (the other component is the alpha channel)
  extract->SetComponents(0);
  extract->SetInput(this->ReconstructedVolume);
  extract->Update();

  reconstructedVolume->DeepCopy(extract->GetOutput());

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::ExtractAlpha(vtkImageData* reconstructedVolume)
{
  if (this->UpdateReconstructedVolume() != PLUS_SUCCESS)
  {
    LOG_ERROR("Failed to load reconstructed volume");
    return PLUS_FAIL;
  }

  vtkSmartPointer<vtkImageExtractComponents> extract = vtkSmartPointer<vtkImageExtractComponents>::New();          

  // Extract the second component (the alpha channel)
  extract->SetComponents(1);
  extract->SetInput(this->ReconstructedVolume);
  extract->Update();

  reconstructedVolume->DeepCopy(extract->GetOutput());

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::SaveReconstructedVolumeToMetafile(const char* filename, bool alpha/*=false*/, bool useCompression/*=true*/)
{
  vtkSmartPointer<vtkImageData> volumeToSave = vtkSmartPointer<vtkImageData>::New();
  if (alpha)
  {
    if (this->ExtractAlpha(volumeToSave) != PLUS_SUCCESS)
    {
      LOG_ERROR("Extracting alpha channel failed!");
      return PLUS_FAIL;
    }
  }
  else
  {
    if (this->ExtractGrayLevels(volumeToSave) != PLUS_SUCCESS)
    {
      LOG_ERROR("Extracting alpha channel failed!");
      return PLUS_FAIL;
    }
  }
  return SaveReconstructedVolumeToMetafile(volumeToSave, filename, useCompression);
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::SaveReconstructedVolumeToMetafile(vtkImageData* volumeToSave, const char* filename, bool useCompression/*=true*/)
{
  if (volumeToSave==NULL)
  {
    LOG_ERROR("vtkVolumeReconstructor::SaveReconstructedVolumeToMetafile: invalid input image");
    return PLUS_FAIL;
  }

  MET_ValueEnumType scalarType = MET_NONE;
  switch (volumeToSave->GetScalarType())
  {
  case VTK_UNSIGNED_CHAR: scalarType = MET_UCHAR; break;
  case VTK_FLOAT: scalarType = MET_FLOAT; break;
  default:
    LOG_ERROR("Scalar type is not supported!");
    return PLUS_FAIL;
  }

  MetaImage metaImage(volumeToSave->GetDimensions()[0], volumeToSave->GetDimensions()[1], volumeToSave->GetDimensions()[2],
                                       volumeToSave->GetSpacing()[0], volumeToSave->GetSpacing()[1], volumeToSave->GetSpacing()[2],
                                       scalarType, 1, volumeToSave->GetScalarPointer());
  metaImage.Origin(volumeToSave->GetOrigin());
  // By definition, LPS orientation in DICOM sense = RAI orientation in MetaIO. See details at:
  // http://www.itk.org/Wiki/Proposals:Orientation#Some_notes_on_the_DICOM_convention_and_current_ITK_usage
  metaImage.AnatomicalOrientation("RAI");
  metaImage.BinaryData(true);
  metaImage.CompressedData(useCompression);
  metaImage.ElementDataFileName("LOCAL");
  if (metaImage.Write(filename) == false)
  {
    LOG_ERROR("Failed to save reconstructed volume in sequence metafile!");
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkVolumeReconstructor::SaveReconstructedVolumeToVtkFile(const char* filename, bool alpha/*=false*/)
{
  vtkSmartPointer<vtkImageData> volumeToSave = vtkSmartPointer<vtkImageData>::New();

  if (alpha)
  {
    if (this->ExtractAlpha(volumeToSave) != PLUS_SUCCESS)
    {
      LOG_ERROR("Extracting alpha channel failed!");
      return PLUS_FAIL;
    }
  }
  else
  {
    if (this->ExtractGrayLevels(volumeToSave) != PLUS_SUCCESS)
    {
      LOG_ERROR("Extracting alpha channel failed!");
      return PLUS_FAIL;
    }
  }

  vtkSmartPointer<vtkDataSetWriter> writer = vtkSmartPointer<vtkDataSetWriter>::New();
  writer->SetFileTypeToBinary();
  writer->SetInput(volumeToSave);
  writer->SetFileName(filename);
  writer->Update();

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
int* vtkVolumeReconstructor::GetClipRectangleOrigin()
{
  return this->Reconstructor->GetClipRectangleOrigin();
}

//----------------------------------------------------------------------------
int* vtkVolumeReconstructor::GetClipRectangleSize()
{
  return this->Reconstructor->GetClipRectangleSize();
}

//----------------------------------------------------------------------------
void vtkVolumeReconstructor::Reset()
{
  this->Reconstructor->ResetOutput();
}

//----------------------------------------------------------------------------
void vtkVolumeReconstructor::SetOutputOrigin(double* origin)
{
  this->Reconstructor->SetOutputOrigin(origin);
}

//----------------------------------------------------------------------------
void vtkVolumeReconstructor::SetOutputSpacing(double* spacing)
{
  this->Reconstructor->SetOutputSpacing(spacing);
}

//----------------------------------------------------------------------------
void vtkVolumeReconstructor::SetOutputExtent(int* extent)
{
  this->Reconstructor->SetOutputExtent(extent);
}