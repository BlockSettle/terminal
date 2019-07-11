#include "ProtobufUtils.h"

#include <google/protobuf/util/json_util.h>

std::string ProtobufUtils::toJson(const google::protobuf::Message &msg, bool addWhitespace)
{
   std::string result;
   google::protobuf::util::JsonOptions options;
   options.add_whitespace = addWhitespace;
   options.preserve_proto_field_names = true;
   google::protobuf::util::MessageToJsonString(msg, &result, options);
   return result;
}

std::string ProtobufUtils::toJsonReadable(const google::protobuf::Message &msg)
{
   return toJson(msg, true);
}

std::string ProtobufUtils::toJsonCompact(const google::protobuf::Message &msg)
{
   return toJson(msg, false);
}
