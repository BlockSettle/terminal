#include <algorithm>
#include "HDPath.h"

using namespace bs;

hd::Path::Path(const std::vector<Elem> &elems) : path_(elems)
{
   isAbsolute_ = (path_[0] == hd::purpose);
   if (isAbsolute_) {
      for (size_t i = 0; i < std::min<size_t>(3, path_.size()); i++) {
         setHardened(i);
      }
   }
}

bool hd::Path::isHardened(size_t index) const
{
   return (hardenedIdx_.find(index) != hardenedIdx_.end());
}

void hd::Path::setHardened(size_t index)
{
   hardenedIdx_.insert(index);
}

hd::Path::Elem hd::Path::get(int index) const
{
   if (path_.empty()) {
      return UINT32_MAX;
   }
   if (index < 0) {
      index += (int)length();
      if (index < 0) {
         return UINT32_MAX;
      }
   }
   else {
      if (index >= length()) {
         return UINT32_MAX;
      }
   }
   return path_[index];
}

void hd::Path::clear()
{
   isAbsolute_ = false;
   path_.clear();
   hardenedIdx_.clear();
}

void hd::Path::append(Elem elem, bool hardened)
{
   path_.push_back(elem);
   if (hardened) {
      setHardened(length() - 1);
   }
}

hd::Path::Elem hd::Path::keyToElem(const std::string &key)
{
   hd::Path::Elem result = 0;
   const std::string &str = (key.length() > 4) ? key.substr(0, 4) : key;
   if (str.empty()) {
      return result;
   }
   for (size_t i = 0; i < str.length(); i++) {
      result |= static_cast<hd::Path::Elem>(str[str.length() - 1 - i]) << (i * 8);
   }
   return result;
}

std::string hd::Path::elemToKey(hd::Path::Elem elem)
{
   std::string result;
   for (size_t i = 4; i > 0; i--) {
      unsigned char c = (elem >> (8 * (i - 1))) & 0xff;
      if (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9'))) {
         result.append(1, c);
      }
   }
   if (result.empty()) {
      result = std::to_string(elem);
   }
   return result;
}

void hd::Path::append(const std::string &key, bool hardened)
{
   append(keyToElem(key), hardened);
}

std::string hd::Path::toString(bool alwaysAbsolute) const
{
   if (path_.empty()) {
      return {};
   }
   std::string result = (alwaysAbsolute || isAbsolute_) ? "m/" : "";
   for (size_t i = 0; i < path_.size(); i++) {
      const auto &elem = path_[i];
      result.append(std::to_string(elem));
      if (isHardened(i)) {
         result.append("'");
      }
      if (i < (path_.size() - 1)) {
         result.append("/");
      }
   }
   return result;
}

static bool isValidPathElem(const std::string &elem)
{
   if (elem.empty()) {
      return false;
   }
   if (elem == "m") {
      return true;
   }
   for (const auto &c : elem) {
      if ((c != '\'') && ((c > '9') || (c < '0'))) {
         return false;
      }
   }
   return true;
}

hd::Path hd::Path::fromString(const std::string &s)
{
   std::string str = s;
   std::vector<std::string>   stringVec;
   size_t cutAt = 0;
   while ((cutAt = str.find('/')) != std::string::npos) {
      if (cutAt > 0) {
         stringVec.push_back(str.substr(0, cutAt));
         str = str.substr(cutAt + 1);
      }
   }
   if (!str.empty()) {
      stringVec.push_back(str);
   }

   Path result;
   for (const auto &elem : stringVec) {
      if ((elem == "m") || !isValidPathElem(elem)) {
         continue;
      }
      const auto pe = static_cast<Elem>(std::stoul(elem));
      result.append(pe, (elem.find("'") != std::string::npos));
   }
   if (result.get(0) == hd::purpose) {
      result.isAbsolute_ = true;
   }
   return result;
}

bool hd::Path::operator < (const hd::Path &other) const
{
   if (length() != other.length()) {
      return (length() < other.length());
   }
   for (size_t i = 0; i < length(); i++) {
      const auto &lval = get((int)i);
      const auto &rval = other.get((int)i);
      if (lval != rval) {
         return (lval < rval);
      }
   }
   return false;
}
