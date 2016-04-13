#include "PlusConfigure.h"
#include "PlusTrackedFrame.h"
#include "igtlPlusUsMessage.h"
#include "igtl_image.h"
#include "igtlutil/igtl_util.h"
#include "igtlutil/igtl_header.h"

namespace igtl
{

  //----------------------------------------------------------------------------
  size_t PlusUsMessage::MessageHeader::GetMessageHeaderSize()
  {
    size_t headersize = 13* sizeof(igtl_uint32); 
    return headersize; 
  }

  //----------------------------------------------------------------------------
  void PlusUsMessage::MessageHeader::ConvertEndianness()
  {
    if (igtl_is_little_endian()) 
    {
      m_DataType = BYTE_SWAP_INT32(m_DataType); 
      m_TransmitFrequency = BYTE_SWAP_INT32(m_TransmitFrequency); 
      m_SamplingFrequency = BYTE_SWAP_INT32(m_SamplingFrequency); 
      m_DataRate = BYTE_SWAP_INT32(m_DataRate); 
      m_LineDensity = BYTE_SWAP_INT32(m_LineDensity);

      m_SteeringAngle = BYTE_SWAP_INT32(m_SteeringAngle);
      m_ProbeID = BYTE_SWAP_INT32(m_ProbeID);
      m_ExtensionAngle = BYTE_SWAP_INT32(m_ExtensionAngle);
      m_Elements = BYTE_SWAP_INT32(m_Elements);
      m_Pitch = BYTE_SWAP_INT32(m_Pitch);

      m_Radius = BYTE_SWAP_INT32(m_Radius);
      m_ProbeAngle = BYTE_SWAP_INT32(m_ProbeAngle);
      m_TxOffset = BYTE_SWAP_INT32(m_TxOffset);
    }
  }

  //----------------------------------------------------------------------------
  PlusUsMessage::PlusUsMessage()
    : ImageMessage()
  {
    this->m_DefaultBodyType = "USMESSAGE";
    this->m_DeviceName ="USMessage";
  }

  //----------------------------------------------------------------------------
  PlusUsMessage::~PlusUsMessage()
  {
  }

  //----------------------------------------------------------------------------
  PlusTrackedFrame& PlusUsMessage::GetTrackedFrame()
  {
    return this->m_TrackedFrame;
  }

  //----------------------------------------------------------------------------
  PlusStatus PlusUsMessage::SetTrackedFrame( const PlusTrackedFrame& trackedFrame ) 
  {
    this->m_TrackedFrame = trackedFrame; 

    double timestamp = this->m_TrackedFrame.GetTimestamp();

    igtl::TimeStamp::Pointer igtlFrameTime = igtl::TimeStamp::New();
    igtlFrameTime->SetTime( timestamp );

    int offset[3]={0}; 
    int imageSizePixels[3]={0}; 
    int size[3]={0}; 

    // NOTE: MUSiiC library expects the frame size in the format 
    // as Ultrasonix provide, not like Plus (Plus: if vector data switch width and 
    // height, because the image is not rasterized like a bitmap, but written rayline by rayline)
    this->m_TrackedFrame.GetFrameSize(size); 
    imageSizePixels[0] = size[1]; 
    imageSizePixels[1] = size[0];
    imageSizePixels[2] = 1;

    int scalarType = PlusVideoFrame::GetIGTLScalarPixelTypeFromVTK( this->m_TrackedFrame.GetImageData()->GetVTKScalarPixelType() ); 

    this->SetDimensions( imageSizePixels );
    this->SetSubVolume(imageSizePixels, offset); 
    this->SetScalarType( scalarType );
    this->SetSpacing(0.2,0.2,1); 
    this->AllocateScalars();

    unsigned char* igtlImagePointer = (unsigned char*)( this->GetScalarPointer() );
    unsigned char* plusImagePointer = (unsigned char*)( this->m_TrackedFrame.GetImageData()->GetScalarPointer() );

    memcpy(igtlImagePointer, plusImagePointer, this->GetImageSize());

    this->SetTimeStamp( igtlFrameTime );

    this->m_MessageHeader.m_DataType = 0; 
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixDataType") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixDataType"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_DataType); 
    }

    this->m_MessageHeader.m_TransmitFrequency = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixTransmitFrequency") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixTransmitFrequency"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_TransmitFrequency); 
    }

    this->m_MessageHeader.m_SamplingFrequency = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixSamplingFrequency") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixSamplingFrequency"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_SamplingFrequency); 
    }

    this->m_MessageHeader.m_DataRate = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixDataRate") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixDataRate"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_DataRate); 
    }

    this->m_MessageHeader.m_LineDensity = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixLineDensity") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixLineDensity"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_LineDensity); 
    }

    this->m_MessageHeader.m_SteeringAngle = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixSteeringAngle") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixSteeringAngle"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_SteeringAngle); 
    }

    this->m_MessageHeader.m_ProbeID = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixProbeID") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixProbeID"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_ProbeID); 
    }

    this->m_MessageHeader.m_ExtensionAngle = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixExtensionAngle") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixExtensionAngle"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_ExtensionAngle); 
    }

    this->m_MessageHeader.m_Elements = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixElements") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixElements"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_Elements); 
    }

    this->m_MessageHeader.m_Pitch = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixPitch") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixPitch"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_Pitch); 
    }

    this->m_MessageHeader.m_Radius = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixRadius") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixRadius"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_Radius); 
    }

    this->m_MessageHeader.m_ProbeAngle = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixProbeAngle") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixProbeAngle"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_ProbeAngle); 
    }

    this->m_MessageHeader.m_TxOffset = 0;
    if ( this->m_TrackedFrame.IsCustomFrameFieldDefined("SonixTxOffset") )
    {
      const char* fieldValue = this->m_TrackedFrame.GetCustomFrameField("SonixTxOffset"); 
      PlusCommon::StringToInt(fieldValue, this->m_MessageHeader.m_TxOffset); 
    }
   
    return PLUS_SUCCESS; 
  }

  //----------------------------------------------------------------------------
  int PlusUsMessage::GetBodyPackSize()
  {
    return GetSubVolumeImageSize() + IGTL_IMAGE_HEADER_SIZE + this->m_MessageHeader.GetMessageHeaderSize();
  }

  //----------------------------------------------------------------------------
  int PlusUsMessage::PackBody()
  {
    igtl::ImageMessage::PackBody();

    MessageHeader* header = (MessageHeader*)(m_Image + GetSubVolumeImageSize() );
    header->m_DataType = this->m_MessageHeader.m_DataType; 
    header->m_TransmitFrequency = this->m_MessageHeader.m_TransmitFrequency; 
    header->m_SamplingFrequency = this->m_MessageHeader.m_SamplingFrequency; 
    header->m_DataRate = this->m_MessageHeader.m_DataRate; 
    header->m_LineDensity = this->m_MessageHeader.m_LineDensity; 

    header->m_SteeringAngle = this->m_MessageHeader.m_SteeringAngle; 
    header->m_ProbeID = this->m_MessageHeader.m_ProbeID; 
    header->m_ExtensionAngle = this->m_MessageHeader.m_ExtensionAngle; 
    header->m_Elements = this->m_MessageHeader.m_Elements; 
    header->m_Pitch = this->m_MessageHeader.m_Pitch; 

    header->m_Radius = this->m_MessageHeader.m_Radius; 
    header->m_ProbeAngle = this->m_MessageHeader.m_ProbeAngle; 
    header->m_TxOffset = this->m_MessageHeader.m_TxOffset; 

    // Convert header endian
    header->ConvertEndianness(); 

    return 1;
  }

  //----------------------------------------------------------------------------
  int PlusUsMessage::UnpackBody()
  {
    igtl::ImageMessage::UnpackBody();

    MessageHeader* header = (MessageHeader*)(m_Image + GetSubVolumeImageSize() );

    // Convert header endian
    header->ConvertEndianness(); 

    this->m_MessageHeader.m_DataType = header->m_DataType; 
    this->m_MessageHeader.m_TransmitFrequency = header->m_TransmitFrequency; 
    this->m_MessageHeader.m_SamplingFrequency = header->m_SamplingFrequency; 
    this->m_MessageHeader.m_DataRate = header->m_DataRate; 
    this->m_MessageHeader.m_LineDensity = header->m_LineDensity; 

    this->m_MessageHeader.m_SteeringAngle = header->m_SteeringAngle; 
    this->m_MessageHeader.m_ProbeID = header->m_ProbeID; 
    this->m_MessageHeader.m_ExtensionAngle = header->m_ExtensionAngle; 
    this->m_MessageHeader.m_Elements = header->m_Elements; 
    this->m_MessageHeader.m_Pitch = header->m_Pitch; 

    this->m_MessageHeader.m_Radius = header->m_Radius; 
    this->m_MessageHeader.m_ProbeAngle = header->m_ProbeAngle; 
    this->m_MessageHeader.m_TxOffset = header->m_TxOffset; 

    return 1;
  }
} //namespace igtl