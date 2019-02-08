#ifndef __BS_ENCRYPTUTILS_ZBASE32_H__
#define __BS_ENCRYPTUTILS_ZBASE32_H__

#include "ZBase32.h"
#include <string>
#include <cassert>

//TODO: move content of this file to other similar functions
namespace bs {
   
   size_t zbase32Encode(const void * data, size_t dataSize, void *dst){
      size_t consumed;
      size_t result = bs::utils::zbase32_encode(static_cast<char*>(dst)
         , static_cast<const uint8_t*>(data), dataSize, consumed);
      assert(consumed == result);
      return result;
   }
   
   template <class T>
   std::string zbase32Encode(const T &data)
   {
      if (data.empty()) {
         return {};
      }
      
      std::string result;
      size_t size = bs::utils::zbase32_encode_estimate_size(data.size());
      result.resize(size);
      size_t resultSize = zbase32Encode(data.data(), data.size(), &result[0]);
      result.resize(resultSize);
      return result;
   }
}



#endif // __BS_ENCRYPTUTILS_ZBASE32_H__
