#include <algorithm>
#include <stdexcept>
#include "HDPath.h"

using namespace bs;

hd::Path::Path(const std::vector<Elem> &elems) : path_(elems)
{
   isAbsolute_ = (path_[0] == hd::purpose);
   if (isAbsolute_) {
      //Goofy path hardening, the nodes should be properly flagged to begin with
      //Only resorting to this so as to not blow up the entire code base. Maybe 
      //it deserves to, however.
      for (size_t i = 0; i < std::min<size_t>(3, path_.size()); i++) {
         path_[i] |= 0x80000000;
      }
   }
}

bool hd::Path::isHardened(size_t index) const
{
   return (path_[index] & 0x80000000);
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
}

void hd::Path::append(Elem elem)
{
   path_.push_back(elem);
}

hd::Path::Elem hd::Path::keyToElem(const std::string &key)
{
   /***
   This is weird af.
   The length restriction is off. It should just fail instead of 
   silently trimming. The size limit does not allow for the harderning
   flag (') with 4 character strings.
   Latin ASCII characters cannot flag the hardening bit but that seems 
   to be a lucky oversight rather than intentional design.

   I recommend against using this. If you want conversions of strings 
   into BIP32 nodes, use a hash method to 4 bytes values rather than
   ASCII to binary conversions.

   Disabling this for now until it is reimplemented with a proper 
   hash function.
   ***/

   throw std::runtime_error("hd::Path::keyToElem disabled");
   
   hd::Path::Elem result = 0;
   const std::string &str = (key.length() > 4) ? key.substr(0, 4) : key;
   if (str.empty() || str.length() > 4) {
      throw std::runtime_error("invalid BIP32 string key");
   }
   for (size_t i = 0; i < str.length(); i++) {
      result |= static_cast<hd::Path::Elem>(str[str.length() - 1 - i]) << (i * 8);
   }
   return result;
}

std::string hd::Path::elemToKey(hd::Path::Elem elem)
{
   //mask off the hardened flag if present
   bool hardened = elem & 0x80000000;
   if (hardened)
      elem &= ~0x80000000;
   std::string result;

   /*** 
   isValidPathElem does not tolerate alphabetical characters, yet path to index conversion
   generates alphabetical nodes. Path to index conversion then back to path from index is 
   at least used to pass addresses from CoreHDLeaf to SyncHDLeaf. Therefor this method is
   not just a pretty printer for BIP32 paths, hence this mismatch in the codec makes no 
   sense and has been disabled. If you want pretty printing, have an extra method for that
   and name it accordingly!
   ***/

/*   for (size_t i = 4; i > 0; i--) {
      unsigned char c = (elem >> (8 * (i - 1))) & 0xff;
      if (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9'))) {
         result.append(1, c);
      }
   }*/

   if (result.empty()) {
      result = std::to_string(elem);
   }
   if (hardened)
      result.append(1, '\'');
   return result;
}

void hd::Path::append(const std::string &key)
{
   append(keyToElem(key));
}

std::string hd::Path::toString() const
{
   if (path_.empty())
      throw std::runtime_error("empty path");

   std::string result = isAbsolute_ ? "m/" : "";
   for (size_t i = 0; i < path_.size(); i++) 
   {
      result.append(elemToKey(path_[i]));
      if (i < (path_.size() - 1)) 
         result.append("/");
   }
   return result;
}

static bool isValidPathElem(const std::string &elem)
{
   if (elem.empty()) {
      return false;
   }
   /*if (elem == "m") {
      return true;
   }*/
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
   for (unsigned i = 0; i < stringVec.size(); i++) 
   {
      auto elem = stringVec[i];
      if ((elem == "m") && i == 0)
         continue;

      if (!isValidPathElem(elem)) 
      {
         /***
         BIP32 path codec is crucial to wallet consistency, it should 
         abort on failures, or at least fail gracefully. Before this 
         throw, it would just continue on errors. This isn't acceptable 
         behavior.
         ***/
         throw std::runtime_error("invalid element in BIP32 path");
      }
      auto pe = static_cast<Elem>(std::stoul(elem));
      if (elem.find("'") != std::string::npos)
         pe |= 0x80000000; //proper way to signify hardness, stick to the spec!
      result.append(pe);
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
