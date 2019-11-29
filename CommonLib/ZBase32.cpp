/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ZBase32.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cassert>

namespace {
   const std::vector<char> EncodingTable = std::vector<char>(
   {'y','b','n','d','r','f','g','8',
    'e','j','k','m','c','p','q','x',
    'o','t','1','u','w','i','s','z',
    'a','3','4','5','h','7','6','9'});
   
   const std::vector<unsigned char> DecodingTable = std::vector<unsigned char>({
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //8
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //16
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //24
      
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //32
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //40
            0xFF, 0x12, 0xFF, 0x19, 0x1A, 0x1B, 0x1E, 0x1D, //48
            0x07, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //56
      
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //64
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //72
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //80
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //88
      
            0xFF, 0x18, 0x01, 0x0C, 0x03, 0x08, 0x05, 0x06, //96
            0x1C, 0x15, 0x09, 0x0A, 0xFF, 0x0B, 0x02, 0x10, //104
            0x0D, 0x0E, 0x04, 0x16, 0x11, 0x13, 0xFF, 0x14, //112
            0x0F, 0x00, 0x17, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  //120
   });
      
}

static bool ignoredSymbol(const char& symbol)
{
   return symbol >= static_cast<int>(DecodingTable.size())
         || DecodingTable[static_cast<size_t>(symbol)] == 0xFF;
}

static size_t createIndex(const char* data, size_t data_length, size_t position, int* index_array)
{
   int j = 0;
   while (j < 8) {
      if (position >= data_length){
         index_array[j++] = -1;
         continue;
      }
      
      if (ignoredSymbol(data[position])){
         position++;
         continue;
      }
      
      index_array[j] = data[position];
      j++;
      position++;
   }
   
   return position;
}

namespace bs {

namespace zbase32_impl {
   size_t zbase32_encode(char output[],
                         const uint8_t input[],
                         size_t input_length,
                         size_t& input_consumed);
   size_t zbase32_encode_estimate_size(size_t size);
   size_t zbase32_decode(uint8_t output[],
                         const char input[],
                         size_t input_length);
   size_t zbase32_decode_estimate_size(size_t size);
} //zbase32_impl

size_t zbase32Encode(const void * data, size_t dataSize, void *dst){
      size_t consumed;
      size_t result = bs::zbase32_impl::zbase32_encode(static_cast<char*>(dst)
            , static_cast<const uint8_t*>(data), dataSize, consumed);
      assert(consumed == result);
   return result;
}

size_t zbase32EncodeEstimateSize(size_t size) {
   return bs::zbase32_impl::zbase32_encode_estimate_size(size);
}
size_t zbase32DecodeEstimateSize(size_t size) {
   return bs::zbase32_impl::zbase32_decode_estimate_size(size);
}
size_t zbase32Decode(const void * data, size_t dataSize, void *dst) {
   try {
      return bs::zbase32_impl::zbase32_decode(static_cast<uint8_t*>(dst), static_cast<const char*>(data), dataSize);
   } catch (const std::exception &) {
      return 0;
   }
}

namespace zbase32_impl {
   size_t zbase32_encode(char output[],
                         const uint8_t input[],
                         size_t input_length,
                         size_t& input_consumed)
   {
      input_consumed = 0;
      if (input == nullptr){
         return 0;
      }

      size_t outIndex = 0;
      for (size_t i = 0; i < input_length; i += 5) {
         size_t byteCount = std::min(static_cast<size_t>(5), static_cast<size_t>(input_length - i));
         
         uint64_t buffer = 0; // Should be >= 5 bytes, because max << (shift) for 5 bytes.
         for (size_t j = 0; j < byteCount; ++j) {
            buffer = (buffer << 8) | input[i+j];
         }
         
         int bitCount = static_cast<int>(byteCount * 8);
         
         while(bitCount > 0){
            size_t index = bitCount >= 5
                        ? static_cast<size_t>(buffer >> (bitCount - 5)) & 0x1f
                        : (buffer & static_cast<uint64_t>(0x1f >> (5 - bitCount))) << (5 - bitCount);
            
            output[outIndex] = EncodingTable[index];
            bitCount -= 5;
            outIndex++;
            input_consumed++;
         }
         
      }
      
      return outIndex;
   }
   
   size_t zbase32_encode_estimate_size(size_t size)
   {
      return static_cast<size_t>(ceilf(size * 8.0f / 5.0f));
   }

   size_t zbase32_decode_estimate_size(size_t size)
   {
      return static_cast<size_t>(ceilf(size * 5.0f / 8.0f));
   }

   size_t zbase32_decode(uint8_t output[], const char input[], size_t input_length)
   {
      if (input_length <= 0) {
         return 0;
      }

      size_t outputIndex = 0;
      int index_array[8] = {0};
      for (size_t pos = 0; pos < input_length;) {
         pos = createIndex(input, input_length, pos, index_array);

         int shortByteCount = 0;
         uint64_t buffer = 0;

         for (size_t j = 0; j < 8 && index_array[j] != -1; ++j) {
            buffer = (buffer << 5) | static_cast<uint64_t>(DecodingTable[static_cast<size_t>(index_array[j])] & 0x1f);
            ++shortByteCount;
         }

         int bitCount = shortByteCount * 5;

         while (bitCount >= 8) {
            uint8_t byte = static_cast<uint8_t>((buffer >> (bitCount - 8)) & 0xff);
            output[outputIndex++] = byte;
            bitCount -= 8;
         }
      }
      return outputIndex;

   }

} //zbase32_impl

} //bs
