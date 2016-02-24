/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

#include "vtkPlusIgtlMessageFactory.h"
#include "vtkObjectFactory.h"
#include "vtkTransformRepository.h" 
#include "vtkTrackedFrameList.h" 
#include "TrackedFrame.h"
#include "vtkMatrix4x4.h"
#include "vtkImageData.h" 
#include "vtksys/SystemTools.hxx"
#include "vtkPlusIgtlMessageCommon.h"
#include "PlusVideoFrame.h" 
#include <typeinfo>

//----------------------------------------------------------------------------
// IGT message types
#include "igtlCommandMessage.h"
#include "igtlImageMessage.h"
#include "igtlPlusClientInfoMessage.h"
#include "igtlPlusTrackedFrameMessage.h"
#include "igtlPlusUsMessage.h"
#include "igtlPositionMessage.h"
#include "igtlStatusMessage.h"
#include "igtlTransformMessage.h"

//----------------------------------------------------------------------------

vtkStandardNewMacro(vtkPlusIgtlMessageFactory);

//----------------------------------------------------------------------------
vtkPlusIgtlMessageFactory::vtkPlusIgtlMessageFactory()
  : IgtlFactory(igtl::MessageFactory::New())
{
  this->IgtlFactory->AddMessageType("CLIENTINFO", (PointerToMessageBaseNew)&igtl::PlusClientInfoMessage::New);
  this->IgtlFactory->AddMessageType("TRACKEDFRAME", (PointerToMessageBaseNew)&igtl::PlusTrackedFrameMessage::New);
  this->IgtlFactory->AddMessageType("USMESSAGE", (PointerToMessageBaseNew)&igtl::PlusUsMessage::New);
}

//----------------------------------------------------------------------------
vtkPlusIgtlMessageFactory::~vtkPlusIgtlMessageFactory()
{
}

//----------------------------------------------------------------------------
void vtkPlusIgtlMessageFactory::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  this->PrintAvailableMessageTypes(os, indent); 
}

//----------------------------------------------------------------------------
vtkPlusIgtlMessageFactory::PointerToMessageBaseNew vtkPlusIgtlMessageFactory::GetMessageTypeNewPointer(const std::string& messageTypeName)
{
  return this->IgtlFactory->GetMessageTypeNewPointer(messageTypeName);
}

//----------------------------------------------------------------------------
void vtkPlusIgtlMessageFactory::PrintAvailableMessageTypes(ostream& os, vtkIndent indent)
{
  os << indent << "Supported OpenIGTLink message types: " << std::endl;
  std::vector<std::string> types;
  this->IgtlFactory->GetAvailableMessageTypes(types);
  for ( std::vector<std::string>::iterator it = types.begin(); it != types.end(); ++it)
  {
    os << indent.GetNextIndent() << "- " << *it << std::endl; 
  }
}

//----------------------------------------------------------------------------
igtl::MessageBase::Pointer vtkPlusIgtlMessageFactory::CreateReceiveMessage(const igtl::MessageHeader::Pointer aIgtlMessageHdr) const
{
  if( aIgtlMessageHdr.IsNull() )
  {
    LOG_ERROR("Null header sent to factory. Unable to produce a message.");
    return NULL;
  }

  igtl::MessageBase::Pointer aMessageBase = this->IgtlFactory->CreateReceiveMessage(aIgtlMessageHdr);
  if( aMessageBase.IsNull() )
  {
    LOG_ERROR("IGTL factory unable to produce message of type:" << aIgtlMessageHdr->GetMessageType());
    return NULL;
  }

  return aMessageBase; 
}

//----------------------------------------------------------------------------
igtl::MessageBase::Pointer vtkPlusIgtlMessageFactory::CreateSendMessage(const std::string& messageType) const
{
  return this->IgtlFactory->CreateSendMessage(messageType);
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusIgtlMessageFactory::PackMessages(const PlusIgtlClientInfo& clientInfo, std::vector<igtl::MessageBase::Pointer>& igtlMessages, TrackedFrame& trackedFrame,
    bool packValidTransformsOnly, vtkTransformRepository* transformRepository/*=NULL*/)
{
  int numberOfErrors = 0; 
  igtlMessages.clear(); 

  if ( transformRepository != NULL )
  {
    transformRepository->SetTransforms(trackedFrame); 
  }

  for ( std::vector<std::string>::const_iterator messageTypeIterator = clientInfo.IgtlMessageTypes.begin(); messageTypeIterator != clientInfo.IgtlMessageTypes.end(); ++ messageTypeIterator )
  {
    std::string messageType = (*messageTypeIterator);
    igtl::MessageBase::Pointer igtlMessage = this->IgtlFactory->CreateSendMessage(messageType);
    if ( igtlMessage.IsNull() )
    {
      LOG_ERROR("Failed to pack IGT messages - unable to create instance from message type: " << messageType ); 
      numberOfErrors++; 
      continue; 
    }

    // Image message 
    if ( typeid(*igtlMessage) == typeid(igtl::ImageMessage) )
    {
      for ( std::vector<PlusIgtlClientInfo::ImageStream>::const_iterator imageStreamIterator = clientInfo.ImageStreams.begin(); imageStreamIterator != clientInfo.ImageStreams.end(); ++imageStreamIterator)
      {
        PlusIgtlClientInfo::ImageStream imageStream = (*imageStreamIterator);
        
        //Set transform name to [Name]To[CoordinateFrame]
        PlusTransformName imageTransformName = PlusTransformName(imageStream.Name, imageStream.EmbeddedTransformToFrame); 

        igtl::Matrix4x4 igtlMatrix;
        if (vtkPlusIgtlMessageCommon::GetIgtlMatrix(igtlMatrix, transformRepository, imageTransformName) != PLUS_SUCCESS)
        {
          LOG_WARNING("Failed to create " << messageType << " message: cannot get image transform");
          numberOfErrors++; 
          continue;
        }

        igtl::ImageMessage::Pointer imageMessage = dynamic_cast<igtl::ImageMessage*>(igtlMessage.GetPointer());
        std::string deviceName = imageTransformName.From() + std::string("_") + imageTransformName.To();
        if( trackedFrame.IsCustomFrameFieldDefined(TrackedFrame::FIELD_FRIENDLY_DEVICE_NAME) )
        {
          // Allow overriding of device name with something human readable
          // The transform name is passed in the metadata
          deviceName = trackedFrame.GetCustomFrameField(TrackedFrame::FIELD_FRIENDLY_DEVICE_NAME);
        }
        imageMessage->SetDeviceName(deviceName.c_str()); 
        if ( vtkPlusIgtlMessageCommon::PackImageMessage(imageMessage, trackedFrame, igtlMatrix) != PLUS_SUCCESS )
        {
          LOG_ERROR("Failed to create " << messageType << " message - unable to pack image message"); 
          numberOfErrors++; 
          continue;
        }
        igtlMessages.push_back( dynamic_cast<igtl::MessageBase*>(imageMessage.GetPointer()) ); 
      }
    }
    // Transform message 
    else if ( typeid(*igtlMessage) == typeid(igtl::TransformMessage) )
    {
      for ( std::vector<PlusTransformName>::const_iterator transformNameIterator = clientInfo.TransformNames.begin(); transformNameIterator != clientInfo.TransformNames.end(); ++transformNameIterator)
      {
        PlusTransformName transformName = (*transformNameIterator);
        bool isValid = false;
        transformRepository->GetTransformValid(transformName, isValid);

        if( !isValid && packValidTransformsOnly )
        {
          LOG_TRACE("Attempted to send invalid transform over IGT Link when server has prevented sending.");
          continue;
        }

        igtl::Matrix4x4 igtlMatrix; 
        if( vtkPlusIgtlMessageCommon::GetIgtlMatrix(igtlMatrix, transformRepository, transformName) == PLUS_FAIL )
        {
          LOG_ERROR("Invalid transform requested from repository: " << transformName);
          return PLUS_FAIL;
        }

        igtl::TransformMessage::Pointer transformMessage = dynamic_cast<igtl::TransformMessage*>(igtlMessage.GetPointer());
        vtkPlusIgtlMessageCommon::PackTransformMessage( transformMessage, transformName, igtlMatrix, trackedFrame.GetTimestamp() );
        igtlMessages.push_back( dynamic_cast<igtl::MessageBase*>(transformMessage.GetPointer()) ); 
      }
    }
    // Position message
    else if ( typeid(*igtlMessage) == typeid(igtl::PositionMessage) )
    {
      for ( std::vector<PlusTransformName>::const_iterator transformNameIterator = clientInfo.TransformNames.begin(); transformNameIterator != clientInfo.TransformNames.end(); ++transformNameIterator)
      {
        /* 
          Advantage of using position message type:
          Although equivalent position and orientation can be described with the TRANSFORM data type, 
          the POSITION data type has the advantage of smaller data size (19%). It is therefore more suitable for 
          pushing high frame-rate data from tracking devices.
        */
        PlusTransformName transformName = (*transformNameIterator);
        igtl::Matrix4x4 igtlMatrix; 
        if( vtkPlusIgtlMessageCommon::GetIgtlMatrix(igtlMatrix, transformRepository, transformName) == PLUS_FAIL )
        {
          LOG_ERROR("Invalid transform requested from repository: " << transformName);
          return PLUS_FAIL;
        }

        float position[3]={igtlMatrix[0][3], igtlMatrix[1][3], igtlMatrix[2][3]};
        float quaternion[4]={0,0,0,1};
        igtl::MatrixToQuaternion( igtlMatrix, quaternion );

        igtl::PositionMessage::Pointer positionMessage = dynamic_cast<igtl::PositionMessage*>(igtlMessage.GetPointer());
        vtkPlusIgtlMessageCommon::PackPositionMessage( positionMessage, transformName, position, quaternion, trackedFrame.GetTimestamp() );
        igtlMessages.push_back( dynamic_cast<igtl::MessageBase*>(positionMessage.GetPointer()) ); 
      }
    }
    // TRACKEDFRAME message
    else if ( typeid(*igtlMessage) == typeid(igtl::PlusTrackedFrameMessage) )
    {
      igtl::PlusTrackedFrameMessage::Pointer trackedFrameMessage = dynamic_cast<igtl::PlusTrackedFrameMessage*>(igtlMessage.GetPointer()); 
      if ( vtkPlusIgtlMessageCommon::PackTrackedFrameMessage(trackedFrameMessage, trackedFrame) != PLUS_SUCCESS )
      {
        LOG_ERROR("Failed to pack IGT messages - unable to pack tracked frame message"); 
        numberOfErrors++; 
        continue;
      }
      igtlMessages.push_back(igtlMessage); 
    }
    // USMESSAGE message
    else if ( typeid(*igtlMessage) == typeid(igtl::PlusUsMessage) )
    {
      igtl::PlusUsMessage::Pointer usMessage = dynamic_cast<igtl::PlusUsMessage*>(igtlMessage.GetPointer()); 
      if ( vtkPlusIgtlMessageCommon::PackUsMessage(usMessage, trackedFrame) != PLUS_SUCCESS )
      {
        LOG_ERROR("Failed to pack IGT messages - unable to pack US message"); 
        numberOfErrors++; 
        continue;
      }
      igtlMessages.push_back(igtlMessage); 
    }
    // String message 
    else if ( typeid(*igtlMessage) == typeid(igtl::StringMessage) )
    {
      for ( std::vector< std::string >::const_iterator stringNameIterator = clientInfo.StringNames.begin(); stringNameIterator != clientInfo.StringNames.end(); ++stringNameIterator)
      {
        const char* stringName = stringNameIterator->c_str();
        const char* stringValue = trackedFrame.GetCustomFrameField(stringName);
        if (stringValue == NULL)
        {
          // no value is available, do not send anything
          continue;
        }
        igtl::StringMessage::Pointer stringMessage = dynamic_cast<igtl::StringMessage*>(igtlMessage.GetPointer());
        vtkPlusIgtlMessageCommon::PackStringMessage( stringMessage, stringName, stringValue, trackedFrame.GetTimestamp() );
        igtlMessages.push_back( dynamic_cast<igtl::MessageBase*>(stringMessage.GetPointer()) ); 
      }
    }
    else if ( typeid(*igtlMessage) == typeid(igtl::CommandMessage) )
    {
      // Is there any use case for the server sending commands to the client?
      igtl::CommandMessage::Pointer commandMessage = dynamic_cast<igtl::CommandMessage*>(igtlMessage.GetPointer());
      //vtkPlusIgtlMessageCommon::PackCommandMessage( commandMessage );
      igtlMessages.push_back( dynamic_cast<igtl::MessageBase*>(commandMessage.GetPointer()) ); 
    }
    else
    {
      LOG_WARNING("This message type (" << messageType << ") is not supported!" ); 
    }
  }

  return ( numberOfErrors == 0 ? PLUS_SUCCESS : PLUS_FAIL );
}
