/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <algorithm>
#include <stdexcept>
#include "BtcUtils.h"
#include "HDPath.h"

namespace bs {
   namespace hd {
      static const std::vector<hd::Purpose> supportedPurposes{
         hd::Purpose::Native, hd::Purpose::Nested, hd::Purpose::NonSegWit
      };
   }
}

using namespace bs;


hd::Path::Path(const std::vector<Elem> &elems)
   : path_(elems), isAbsolute_(false)
{
   for (const auto &purpose : hd::supportedPurposes) {
      if (path_[0] == static_cast<Elem>(purpose)) {
         isAbsolute_ = true;
         break;
      }
   }
   if (isAbsolute_) {
      //Goofy path hardening, the nodes should be properly flagged to begin with
      //Only resorting to this so as to not blow up the entire code base. Maybe 
      //it deserves to, however.
      for (size_t i = 0; i < std::min<size_t>(3, path_.size()); i++) {
         path_[i] |= hardFlag;
      }
   }
}

bool hd::Path::isHardened(size_t index) const
{
   return (path_[index] & hardFlag);
}

void hd::Path::setHardened(size_t index, bool value)
{
   if (value) {
      path_[index] |= hardFlag;
   }
   else {
      path_[index] &= ~hardFlag;
   }
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
   if (key.empty()) {
      throw PathException("empty string key");
   }
   const auto hash = BtcUtils::getSha256(key);
   hd::Path::Elem result = 0;
   for (int startIdx = 0; startIdx < hash.getSize() - 4; ++startIdx) {
      result = BinaryData::StrToIntBE<hd::Path::Elem>(hash.getSliceCopy(startIdx, 4));
      if ((result & hardFlag) == hardFlag) {
         result &= ~hardFlag;
      }
      bool isResultClashingPredefinedElems = false;
      for (const hd::Path::Elem elem : { 0, 1, 0x4253, 0x41757468 }) {
         if (result == elem) {
            isResultClashingPredefinedElems = true;
            break;
         }
      }
      if (isResultClashingPredefinedElems) {
         result = 0;
      }
      else {
         break;
      }
   }
   if (result == 0) {
      throw PathException("failed to generate index from key");
   }
   return result;
}

std::string hd::Path::elemToKey(hd::Path::Elem elem)
{
   //mask off the hardened flag if present
   bool hardened = elem & hardFlag;
   if (hardened)
      elem &= ~hardFlag;
   std::string result;

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
   if (path_.empty()) {
      throw PathException("empty path");
   }
   std::string result = isAbsolute_ ? "m/" : "";
   for (size_t i = 0; i < path_.size(); i++) {
      result.append(elemToKey(path_[i]));
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
   for (unsigned i = 0; i < stringVec.size(); i++) {
      auto elem = stringVec[i];
      if ((elem == "m") && (i == 0)) {
         continue;
      }
      if (!isValidPathElem(elem)) {
         /***
         BIP32 path codec is crucial to wallet consistency, it should 
         abort on failures, or at least fail gracefully. Before this 
         throw, it would just continue on errors. This isn't acceptable 
         behavior.
         ***/
         throw PathException("invalid element in BIP32 path");
      }
      auto pe = static_cast<Elem>(std::stoul(elem));
      if (elem.find("'") != std::string::npos) {
         pe |= hardFlag; //proper way to signify hardness, stick to the spec!
      }
      result.append(pe);
   }

   const auto firstElem = result.get(0) & ~hardFlag;
   for (const auto &purpose : hd::supportedPurposes) {
      if (firstElem == static_cast<Elem>(purpose)) {
         result.isAbsolute_ = true;
      }
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


hd::Purpose bs::hd::purpose(AddressEntryType aet)
{
   switch (aet) {
   case AddressEntryType_Default:
   case AddressEntryType_P2WPKH:
      return hd::Purpose::Native;
   case AddressEntryType_P2SH:
   case static_cast<AddressEntryType>(AddressEntryType_P2SH | AddressEntryType_P2WPKH):
      return hd::Purpose::Nested;
   case AddressEntryType_P2PKH:
      return hd::Purpose::NonSegWit;
   default:
      throw PathException("failed to get purpose for address type "
         + std::to_string((int)aet));
   }
}

AddressEntryType bs::hd::addressType(hd::Path::Elem purpose)
{
   switch (static_cast<hd::Purpose>(purpose & ~hardFlag)) {
   case Purpose::Native:
      return AddressEntryType_P2WPKH;
   case Purpose::Nested:
      return static_cast<AddressEntryType>(AddressEntryType_P2SH | AddressEntryType_P2WPKH);
   case Purpose::NonSegWit:
      return AddressEntryType_P2PKH;
   default:
      throw PathException("failed to get address type for purpose");
   }
}
