#ifndef __BS_ZBASE32_H__
#define __BS_ZBASE32_H__
#include <inttypes.h>
#include <string>

namespace bs {
   namespace utils {
      size_t zbase32_encode(char output[],
                            const uint8_t input[],
                            size_t input_length,
                            size_t& input_consumed);
      size_t zbase32_encode_estimate_size(size_t size);
      size_t zbase32_decode(uint8_t output[],
                            const char input[],
                            size_t input_length);
      size_t zbase32_decode_estimate_size(size_t size);
   }

}

#endif // __BS_ZBASE32_H__
