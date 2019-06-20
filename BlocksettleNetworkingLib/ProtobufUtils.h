#ifndef PROTOBUF_UTILS_H
#define PROTOBUF_UTILS_H

#include <google/protobuf/message.h>

class ProtobufUtils
{
public:
   static std::string toJson(const google::protobuf::Message &msg);
};

#endif
