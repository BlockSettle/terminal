#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>

namespace bs {

   std::string toHex(const std::string &str, bool uppercase = true);

   // Works for ASCII encoding only
   std::string toLower(std::string str);
   std::string toUpper(std::string str);

   // Very basic email address verification check (checks that there is one '@' symbol and at least one '.' after that)
   bool isValidEmail(const std::string &str);

} // namespace bs

#endif
