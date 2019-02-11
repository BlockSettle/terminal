#include <cassert>
#include "ZBase32.h"

namespace bs {
   size_t zbase32Encode(const void * data, size_t dataSize, void *dst){
         size_t consumed;
         size_t result = bs::utils::zbase32_encode(static_cast<char*>(dst)
               , static_cast<const uint8_t*>(data), dataSize, consumed);
         assert(consumed == result);
      return result;
   }

   size_t zbase32EncodeEstimateSize(size_t size) {
      return bs::utils::zbase32_encode_estimate_size(size);
   }
   size_t zbase32DecodeEstimateSize(size_t size) {
      return bs::utils::zbase32_decode_estimate_size(size);
   }
   size_t zbase32Decode(const void * data, size_t dataSize, void *dst) {
      try {
         return bs::utils::zbase32_decode(static_cast<uint8_t*>(dst), static_cast<const char*>(data), dataSize);
      } catch (const std::exception &) {
         return 0;
      }
   }

}
