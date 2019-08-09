#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <cinttypes>
#include <string>
#include <vector>

namespace bs {

   std::string toHex(const std::string &str, bool uppercase = true);

   // Works for ASCII encoding only
   std::string toLower(std::string str);
   std::string toUpper(std::string str);

} // namespace bs

#endif
