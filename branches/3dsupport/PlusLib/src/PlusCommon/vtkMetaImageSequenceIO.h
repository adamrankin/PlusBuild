/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkMetaImageSequenceIO_h
#define __vtkMetaImageSequenceIO_h

#include "vtkPlusCommonExport.h"

#ifdef _MSC_VER
#pragma warning ( disable : 4786 )
#endif 

#include "PlusVideoFrame.h" // for US_IMAGE_ORIENTATION

class vtkTrackedFrameList;
class TrackedFrame;

/*!
  \class vtkMetaImageSequenceIO
  \brief Read and write MetaImage file with a sequence of frames, with additional information for each frame
  \ingroup PlusLibCommon
*/
class vtkPlusCommonExport vtkMetaImageSequenceIO : public vtkObject
{
public:

  static vtkMetaImageSequenceIO *New();
  vtkTypeMacro(vtkMetaImageSequenceIO, vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent);

  /*! Set the TrackedFrameList where the images are stored */
  virtual void SetTrackedFrameList(vtkTrackedFrameList *trackedFrameList);
  /*! Get the TrackedFrameList where the images are stored */
  vtkGetObjectMacro(TrackedFrameList, vtkTrackedFrameList);

  /*! Accessors to control 2D Dims output */
  vtkSetMacro(Output2DDataWithZDimensionIncluded, bool);
  vtkGetMacro(Output2DDataWithZDimensionIncluded, bool);

  /*!
    Set/get the ultrasound image orientation for file storage (as the result of writing).
    Note that the B-mode image data shall be always stored in MF orientation in the TrackedFrameList object in memory.
    The ultrasound image axes are defined as follows:
    * x axis: points towards the x coordinate increase direction
    * y axis: points towards the y coordinate increase direction
  */  
  vtkSetMacro(ImageOrientationInFile, US_IMAGE_ORIENTATION);

  /*!
    Set/get the ultrasound image orientation for memory storage (as the result of reading).
    B-mode image data shall be always stored in MF orientation in the TrackedFrameList object in memory.
    The ultrasound image axes are defined as follows:
    * x axis: points towards the x coordinate increase direction
    * y axis: points towards the y coordinate increase direction
  */  
  vtkSetMacro(ImageOrientationInMemory, US_IMAGE_ORIENTATION);

  /*! Write object contents into file */
  virtual PlusStatus Write(bool removeImageData=false);

  /*! Read file contents into the object */
  virtual PlusStatus Read();

  /*! Prepare the sequence for writing */
  virtual PlusStatus PrepareHeader(bool removeImageData=false);

  /*! 
    Append the frames in tracked frame list to the header, if the onlyTrackerData flag is true it will not save
    in the header the image data related fields. 
  */
  virtual PlusStatus AppendImagesToHeader(bool onlyTrackerData = false);

  /*! Finalize the header */
  virtual PlusStatus FinalizeHeader();

  /*! Write images to disc, compression allowed */
  virtual PlusStatus WriteImages();

  /*! Append image data to the sequence, compression not allowed */
  virtual PlusStatus AppendImages();

  /*! Close the sequence */
  virtual PlusStatus Close();

  /*! Check if this class can read the specified file */
  virtual bool CanReadFile(const char*);

  /*! Returns a pointer to a single frame */
  virtual TrackedFrame* GetTrackedFrame(int frameNumber);

  /*! Update a field in the image header with its current value */
  PlusStatus UpdateFieldInImageHeader(const char* fieldName);

  /*!
    Set input/output file name. The file contains only the image header in case of
    MHD images and the full image (including pixel data) in case of MHA images.
  */
  virtual PlusStatus SetFileName(const char* aFilename);

  /*! Flag to enable/disable compression of image data */
  vtkGetMacro(UseCompression, bool);
  /*! Flag to enable/disable compression of image data */
  vtkSetMacro(UseCompression, bool);
  /*! Flag to enable/disable compression of image data */
  vtkBooleanMacro(UseCompression, bool);

  /*! Return the dimensions of the sequence */
  vtkGetMacro(Dimensions, int*);

protected:
  vtkMetaImageSequenceIO();
  virtual ~vtkMetaImageSequenceIO();

  /*! Opens a file. Doesn't log error if it fails because it may be expected. */
  PlusStatus FileOpen(FILE **stream, const char* filename, const char* flags);

  /*! Set a custom string field value for a specific frame */
  PlusStatus SetCustomFrameString(int frameNumber, const char* fieldName,  const char* fieldValue);

  /*! Delete custom frame field from tracked frame */
  PlusStatus DeleteCustomFrameString(int frameNumber, const char* fieldName); 
  
  /*! Get a custom string field value for a specific frame */
  bool SetCustomString(const char* fieldName, const char* fieldValue);

  /*! Get a custom string field value (global, not for a specific frame) */
  const char* GetCustomString(const char* fieldName);

  /*! Read all the fields in the metaimage file header */
  virtual PlusStatus ReadImageHeader();

  /*! Read pixel data from the metaimage */
  virtual PlusStatus ReadImagePixels();

  /*! Write all the fields to the metaimage file header */
  virtual PlusStatus OpenImageHeader(bool removeImageData=false);

  /*! Write pixel data to the metaimage */
  virtual PlusStatus WriteImagePixels(const std::string& aFilename, bool forceAppend = false, bool removeImageData=false);

  /*! 
    Convenience function that extends the tracked frame list (if needed) to make sure
    that the requested frame is included in the list
  */
  virtual void CreateTrackedFrameIfNonExisting(unsigned int frameNumber);
  
  /*! Get the largest possible image size in the tracked frame list */
  virtual void GetMaximumImageDimensions(int maxFrameSize[3]); 

  /*! Get full path to the file for storing the pixel data */
  std::string GetPixelDataFilePath();
  /*! Conversion between ITK and METAIO pixel types */
  PlusStatus ConvertMetaElementTypeToVtkPixelType(const std::string &elementTypeStr, PlusCommon::VTKScalarPixelType &vtkPixelType);
  /*! Conversion between ITK and METAIO pixel types */
  PlusStatus ConvertVtkPixelTypeToMetaElementType(PlusCommon::VTKScalarPixelType vtkPixelType, std::string &elementTypeStr);

  /*! 
    Writes the compressed pixel data directly into file. 
    The compression is performed in chunks, so no excessive memory is used for the compression.
    \param outputFileStream the file stream where the compressed pixel data will be written to
    \param compressedDataSize returns the size of the total compressed data that is written to the file.
  */
  virtual PlusStatus WriteCompressedImagePixelsToFile(FILE *outputFileStream, int &compressedDataSize, bool removeImageData=false);

  /*! Copy from file A to B */
  virtual PlusStatus MoveDataInFiles(const std::string& sourceFilename, const std::string& destFilename, bool append);
private:

#ifdef _WIN32
  typedef __int64 FilePositionOffsetType;
#elif defined __APPLE__
  typedef off_t FilePositionOffsetType;
#else
  typedef off64_t FilePositionOffsetType;
#endif
    
  /*! Custom frame fields and image data are stored in the m_FrameList for each frame */
  vtkTrackedFrameList* TrackedFrameList;

  /*! Name of the file that contains the image header (*.MHA or *.MHD) */
  std::string FileName;
  /*! Name of the temporary file used to build up the header */
  std::string TempHeaderFileName;
  /*! Name of the temporary file used to build up the image data */
  std::string TempImageFileName;
  /*! Enable/disable zlib compression of pixel data */
  bool UseCompression;
  /*! ASCII or binary */
  bool IsPixelDataBinary;
  /*! Integer/float, short/long, signed/unsigned */
  PlusCommon::VTKScalarPixelType PixelType;
  /*! Number of components (or channels) */
  int NumberOfScalarComponents;
  /*! Number of image dimensions. Only 2 (single frame) or 3 (sequence of frames) or 4 (sequence of volumes) are supported. */
  int NumberOfDimensions;
  /*! Frame size (first three elements) and number of frames (last element) */
  int Dimensions[4];
  /*! Current frame offset, this is used to build up frames one addition at a time */
  int CurrentFrameOffset;
  /*! If 2D data, boolean to determine if we should write out in the form X Y Nfr (false) or X Y 1 Nfr (true) */
  bool Output2DDataWithZDimensionIncluded;
  /*! Total bytes written */
  unsigned long long TotalBytesWritten;

  /*! 
    Image orientation in memory is always MF for B-mode, but when reading/writing a file then
    any orientation can be used.
  */
  US_IMAGE_ORIENTATION ImageOrientationInFile;

  /*! 
    Image orientation for reading into memory.
  */
  US_IMAGE_ORIENTATION ImageOrientationInMemory;

  /*! 
    Image type (B-mode, RF, ...)
  */
  US_IMAGE_TYPE ImageType;

  /*! Position of the first pixel of the image data within the pixel data file */
  FilePositionOffsetType PixelDataFileOffset;
  /*! File name where the pixel data is stored */
  std::string PixelDataFileName;
  
  vtkMetaImageSequenceIO(const vtkMetaImageSequenceIO&); //purposely not implemented
  void operator=(const vtkMetaImageSequenceIO&); //purposely not implemented
};

#endif // __vtkMetaImageSequenceIO_h 