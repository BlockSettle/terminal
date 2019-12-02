/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __BS_ZBASE32_H__
#define __BS_ZBASE32_H__
#include <inttypes.h>
#include <string>

namespace bs {
   size_t zbase32EncodeEstimateSize(size_t size);
   size_t zbase32Encode(const void * data, size_t dataSize, void *dst);
   size_t zbase32DecodeEstimateSize(size_t size);
   size_t zbase32Decode(const void * data, size_t dataSize, void *dst);

   template <class T, class T2>
   T zbase32Encode(const T2 &data)
   {
      static_assert(sizeof(typename T::value_type) == 1, "only int8_t and uint8_t supported");
      static_assert(sizeof(typename T2::value_type) == 1, "only int8_t and uint8_t supported");

      T result;
      result.resize(zbase32EncodeEstimateSize(data.size()));
      size_t resultSize = zbase32Encode(data.data(), data.size(), result.data());
      result.resize(resultSize);
      return result;
   }

   template <class T>
   std::string zbase32Encode(const T &data)
   {
      if (data.empty()) {
         return {};
      }

      std::string result;
      size_t size = zbase32EncodeEstimateSize(data.size());
      result.resize(size);
      size_t resultSize = zbase32Encode(data.data(), data.size(), &result[0]);
      result.resize(resultSize);
      return result;
   }

   template <class T, class T2>
   T zbase32Decode(const T2 &data)
   {
      static_assert(sizeof(typename T::value_type) == 1, "only int8_t and uint8_t supported");
      static_assert(sizeof(typename T2::value_type) == 1, "only int8_t and uint8_t supported");

      T result;
      result.resize(zbase32DecodeEstimateSize(data.size()));
      size_t resultSize = zbase32Decode(data.data(), data.size(), result.data());
      result.resize(resultSize);
      return result;
   }

}

#endif // __BS_ZBASE32_H__
