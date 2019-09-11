#include "OtcUtils.h"


namespace {

   const std::string kSerializePrefix = "OTC:";

} // namespace

std::string OtcUtils::serializeMessage(const BinaryData &data)
{
   return kSerializePrefix + data.toHexStr();
}

BinaryData OtcUtils::deserializeMessage(const std::string &data)
{
   size_t pos = data.find(kSerializePrefix);
   if (pos != 0) {
      return {};
   }
   try {
      return BinaryData::CreateFromHex(data.substr(kSerializePrefix.size()));
   } catch(...) {
      return {};
   }
}
