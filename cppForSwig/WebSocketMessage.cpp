////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2018, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "WebSocketMessage.h"
#include "libwebsockets.h"
#include <google/protobuf/io/zero_copy_stream_impl.h>

using namespace ::google::protobuf::io;

////////////////////////////////////////////////////////////////////////////////
vector<BinaryData> WebSocketMessage::serialize(uint32_t id,
   const vector<uint8_t>& payload)
{
   BinaryDataRef bdr;
   if(payload.size() > 0)
      bdr.setRef(&payload[0], payload.size());
   return serialize(id, bdr);
}

////////////////////////////////////////////////////////////////////////////////
vector<BinaryData> WebSocketMessage::serialize(uint32_t id,
   const string& payload)
{
   BinaryDataRef bdr((uint8_t*)payload.c_str(), payload.size());
   return serialize(id, bdr);
}

////////////////////////////////////////////////////////////////////////////////
vector<BinaryData> WebSocketMessage::serialize(uint32_t id, 
   const BinaryDataRef& payload)
{
   //TODO: fallback to raw binary messages once lws is standardized
   //TODO: less copies, more efficient serialization

   vector<BinaryData> result;

   size_t data_size =
      WEBSOCKET_MESSAGE_PACKET_SIZE - 
      WEBSOCKET_MESSAGE_PACKET_HEADER - 
      3; //max varint length to encode size <= 8000
   auto msg_count = payload.getSize() / data_size;
   if (msg_count * data_size < payload.getSize())
      msg_count++;

   if (msg_count > 255)
      throw LWS_Error("msg is too large");

   //for empty return (client method may still be waiting on reply to return)
   if (msg_count == 0)
      msg_count = 1;

   size_t pos = 0;
   for (unsigned i = 0; i < msg_count; i++)
   {
      BinaryWriter bw;

      //leading bytes for lws write routine
      BinaryData bd_buffer(LWS_PRE);
      bw.put_BinaryData(bd_buffer); 

      //msg id
      bw.put_uint32_t(id);

      //packet counts and id
      bw.put_uint8_t(msg_count);
      bw.put_uint8_t(i);

      //msg size
      auto size = min(data_size, payload.getSize() - pos);
      bw.put_var_int(size);

      //data
      if (payload.getSize() > 0)
      {
         BinaryDataRef bdr(payload.getPtr() + pos, size);
         bw.put_BinaryDataRef(bdr);
         pos += size;
      }

      result.push_back(bw.getData());
   }

   return result;
}

////////////////////////////////////////////////////////////////////////////////
bool WebSocketMessage::reconstructFragmentedMessage(
   const map<uint8_t, BinaryDataRef>& payloadMap,
   shared_ptr<::google::protobuf::Message> msg)
{
   //this method expects packets in order

   if (payloadMap.size() == 0)
      return false;

   auto count = payloadMap.size();

   //create a zero copy stream from each packet
   vector<ZeroCopyInputStream*> streams;
   streams.reserve(count);
   
   try
   {
      for (auto& data_pair : payloadMap)
      {
         BinaryRefReader brr(data_pair.second);
         brr.advance(WEBSOCKET_MESSAGE_PACKET_HEADER);
         auto len = brr.get_var_int();
         auto bdrPacket = brr.get_BinaryDataRef(len);

         auto stream = new ArrayInputStream(
            bdrPacket.getPtr(), bdrPacket.getSize());
         streams.push_back(stream);
      }
   }
   catch (...)
   {
      for (auto& stream : streams)
         delete stream;
      return false;
   }

   //pass it all to concatenating stream
   ConcatenatingInputStream cStream(&streams[0], streams.size());

   //deser message
   auto result = msg->ParseFromZeroCopyStream(&cStream);

   //cleanup
   for (auto& stream : streams)
      delete stream;

   return result;
}

////////////////////////////////////////////////////////////////////////////////
bool WebSocketMessage::reconstructFragmentedMessage(
   const map<uint8_t, BinaryDataRef>& payloadMap, BinaryData& msg)
{
   //this method expects packets in order

   if (payloadMap.size() == 0)
      return false;

   try
   {
      for (auto& data_pair : payloadMap)
      {
         BinaryRefReader brr(data_pair.second);
         brr.advance(WEBSOCKET_MESSAGE_PACKET_HEADER);
         auto len = brr.get_var_int();
         auto bdrPacket = brr.get_BinaryDataRef(len);

         msg.append(bdrPacket);
      }
   }
   catch (...)
   {
      return false;
   }

   return true;
}

////////////////////////////////////////////////////////////////////////////////
uint32_t WebSocketMessage::getMessageId(const BinaryDataRef& packet)
{
   //sanity check
   if (packet.getSize() < WEBSOCKET_MESSAGE_PACKET_HEADER ||
      packet.getSize() > WEBSOCKET_MESSAGE_PACKET_SIZE)
      return UINT32_MAX;

   return *(uint32_t*)packet.getPtr();
}

////////////////////////////////////////////////////////////////////////////////
uint8_t WebSocketMessage::getPayloadId(const BinaryDataRef& packet)
{
   //sanity check
   if (packet.getSize() < WEBSOCKET_MESSAGE_PACKET_HEADER ||
      packet.getSize() > WEBSOCKET_MESSAGE_PACKET_SIZE)
      return UINT32_MAX;

   return *(packet.getPtr() + 5);
}

////////////////////////////////////////////////////////////////////////////////
vector<pair<unsigned, BinaryDataRef>> WebSocketMessage::parsePacket(
   BinaryDataRef packet)
{
   vector<pair<unsigned, BinaryDataRef>> msgVec;
   BinaryRefReader brr(packet);

   while (brr.getSizeRemaining() > 0)
   {
      if (brr.getSizeRemaining() < WEBSOCKET_MESSAGE_PACKET_HEADER + 1)
         break;

      brr.advance(WEBSOCKET_MESSAGE_PACKET_HEADER);

      try
      {
         uint8_t varIntSize;
         auto len = brr.get_var_int(&varIntSize);
         if (brr.getSizeRemaining() < len)
            break;

         brr.rewind(varIntSize + WEBSOCKET_MESSAGE_PACKET_HEADER);
         auto&& msgRef = brr.get_BinaryDataRef(
            WEBSOCKET_MESSAGE_PACKET_HEADER + len + varIntSize);
         auto msgId = getMessageId(msgRef);

         auto&& data_pair = make_pair(msgId, msgRef);
         msgVec.push_back(move(data_pair));
      }
      catch (runtime_error&)
      {
         break;
      }
   }

   return msgVec;
}

////////////////////////////////////////////////////////////////////////////////
uint8_t WebSocketMessage::getMessageCount(const BinaryDataRef& bdr)
{
   return *(bdr.getPtr() + 4);
}

////////////////////////////////////////////////////////////////////////////////
BinaryDataRef WebSocketMessage::getSingleMessage(const BinaryDataRef& bdr)
{
   BinaryRefReader brr(bdr);
   brr.advance(WEBSOCKET_MESSAGE_PACKET_HEADER);
   auto len = brr.get_var_int();
   return brr.get_BinaryDataRef(len);
}

///////////////////////////////////////////////////////////////////////////////
//
// FragmentedMessage
//
///////////////////////////////////////////////////////////////////////////////
void FragmentedMessage::mergePayload(BinaryDataRef payload)
{
   if (payload.getSize() == 0)
      return;

   auto id = WebSocketMessage::getPayloadId(payload);
   payloads_.insert(move(make_pair(id, payload)));
}

///////////////////////////////////////////////////////////////////////////////
bool FragmentedMessage::getMessage(
   shared_ptr<::google::protobuf::Message> msgPtr)
{
   if (!isComplete())
      return false;

   return WebSocketMessage::reconstructFragmentedMessage(payloads_, msgPtr);
}

///////////////////////////////////////////////////////////////////////////////
bool FragmentedMessage::getMessage(BinaryData& bd)
{
   return WebSocketMessage::reconstructFragmentedMessage(payloads_, bd);
}

///////////////////////////////////////////////////////////////////////////////
bool FragmentedMessage::isComplete() const
{
   if (payloads_.size() == 0)
      return false;

   auto iter = payloads_.begin();
   auto count = WebSocketMessage::getMessageCount(iter->second);

   if (payloads_.size() != count)
      return false;

   return true;
}
