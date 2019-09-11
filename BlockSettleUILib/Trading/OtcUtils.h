#ifndef OTC_UTILS_H
#define OTC_UTILS_H

#include <string>
#include "BinaryData.h"

class OtcUtils
{
public:
   static std::string serializeMessage(const BinaryData &data);
   static BinaryData deserializeMessage(const std::string &data);

};

#endif
