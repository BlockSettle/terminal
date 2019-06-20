#include "ProtobufUtils.h"

#include <google/protobuf/util/json_util.h>

std::string ProtobufUtils::toJson(const google::protobuf::Message &msg)
{
   std::string result;
   google::protobuf::util::JsonOptions options;
   options.add_whitespace = true;
   options.preserve_proto_field_names = true;
   google::protobuf::util::MessageToJsonString(msg, &result, options);
   return result;
}
