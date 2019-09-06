#ifndef PROTOBUF_UTILS_H
#define PROTOBUF_UTILS_H

#include <google/protobuf/message.h>
#include <google/protobuf/any.pb.h>

class ProtobufUtils
{
public:
   static std::string toJson(const google::protobuf::Message &msg, bool addWhitespace = true);
   static std::string toJsonReadable(const google::protobuf::Message &msg);
   static std::string toJsonCompact(const google::protobuf::Message &msg);
   static std::string pbMessageToString(const google::protobuf::Message& msg);
   template<typename T>
   static bool pbAnyToMessage(const google::protobuf::Any& any, google::protobuf::Message* msg);
   template<typename T>
   static bool pbStringToMessage(const std::string& packetString, google::protobuf::Message* msg);
};

template<typename T>
bool ProtobufUtils::pbAnyToMessage(const google::protobuf::Any& any, google::protobuf::Message* msg)
{
   if (any.Is<T>())
   {
      if (!any.UnpackTo(msg))
      {
         return false;
      }

      return true;
   }

   return false;
}

template<typename T>
bool ProtobufUtils::pbStringToMessage(const std::string& packetString, google::protobuf::Message* msg)
{
   google::protobuf::Any any;
   any.ParseFromString(packetString);

   if (any.Is<T>())
   {
      if (!any.UnpackTo(msg))
      {
         return false;
      }

      return true;
   }

   return false;
}

#endif
