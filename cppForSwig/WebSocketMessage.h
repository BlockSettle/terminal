////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2018, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _WEBSOCKET_MESSAGE_H
#define _WEBSOCKET_MESSAGE_H

#include <stdexcept>
#include <string>
#include <memory>

#include "BinaryData.h"
#include <google/protobuf/message.h>

#define WEBSOCKET_MESSAGE_PACKET_SIZE 8000
#define WEBSOCKET_MESSAGE_PACKET_HEADER 6
#define WEBSOCKET_CALLBACK_ID 0xFFFFFFFE

using namespace std;

class LWS_Error : public runtime_error
{
public:
   LWS_Error(const string& err) :
      runtime_error(err)
   {}
};

class WebSocketMessage
{
public:
   static vector<BinaryData> serialize(uint32_t, const BinaryDataRef&);
   static vector<BinaryData> serialize(uint32_t, const vector<uint8_t>&);
   static vector<BinaryData> serialize(uint32_t, const string&);
   
   static uint8_t getMessageCount(const BinaryDataRef&);
   static uint32_t getMessageId(const BinaryDataRef&);
   static uint8_t getPayloadId(const BinaryDataRef&);
   
   static vector<pair<unsigned, BinaryDataRef>> parsePacket(BinaryDataRef);
 
   static BinaryDataRef getSingleMessage(const BinaryDataRef&);
   static bool reconstructFragmentedMessage(
      const map<uint8_t, BinaryDataRef>&,
      shared_ptr<::google::protobuf::Message>);
   static bool reconstructFragmentedMessage(
      const map<uint8_t, BinaryDataRef>&, BinaryData&);
};

///////////////////////////////////////////////////////////////////////////////
struct FragmentedMessage
{
   map<uint8_t, BinaryDataRef> payloads_;

   void mergePayload(BinaryDataRef);
   bool getMessage(shared_ptr<::google::protobuf::Message>);
   bool getMessage(BinaryData&);
   bool isComplete(void) const;
};

#endif
