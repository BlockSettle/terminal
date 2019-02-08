#include "ZBase32.h"
#include <vector>
#include <string>
#include <cmath>
namespace {
   const std::vector<char> EncodingTable = std::vector<char>(
   {'y','b','n','d','r','f','g','8',
    'e','j','k','m','c','p','q','x',
    'o','t','1','u','w','i','s','z',
    'a','3','4','5','h','7','6','9'});
   
   std::vector<unsigned char> DecodingTable = std::vector<unsigned char>(128, static_cast<unsigned char>(255));
   
   //TODO; fill this table and reuse, 
   std::vector<unsigned char> DecodingTable1 = std::vector<unsigned char>({
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //8
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //16
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //24
      
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //32
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //40
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //48
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //56
      
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //64
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //72
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //80
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //88
      
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //96
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //104
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //112
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  //120
   });
      
}

static bool ignoredSymbol(const char& symbol)
{
   return symbol >= DecodingTable.size() || DecodingTable[static_cast<size_t>(symbol)] == 255;
}

static size_t createIndex(const std::string& data, size_t position, int* index)
{
   int j = 0;
   while (j < 8) {
      if (position >= data.length()){
         index[j++] = -1;
         continue;
      }
      
      if (ignoredSymbol(data.at(position))){
         position++;
         continue;
      }
      
      index[j] = data.at(position);
      j++;
      position++;
   }
   
   return position;
}



namespace bs { namespace utils {
   size_t zbase32_encode(char output[],
                         const uint8_t input[],
                         size_t input_length,
                         size_t& input_consumed)
   {
      input_consumed = 0;
      if (input == nullptr){
         return 0;
      }
      
      //std::vector<uint8_t> encodedResult;
      size_t size = zbase32_encode_estimate_size(input_length);
      //encodedResult.resize(size);
      size_t outIndex = 0;
      for (int i = 0; i < input_length; i += 5) {
         int byteCount = std::min(5, static_cast<int>(input_length - i));
         
         size_t buffer = 0;
         for (int j = 0; j < byteCount; ++j) {
            buffer = (buffer << 8) | input[i+j];
         }
         
         int bitCount = byteCount * 8;
         
         while(bitCount > 0){
            size_t index = bitCount >= 5
                        ? (size_t)(buffer >> (bitCount - 5)) & 0x1f
                        : (size_t)(buffer & (size_t)(0x1f >> (5 - bitCount))) << (5 - bitCount);
            
            //encodedResult.push_back(EncodingTable[index]);
            output[outIndex] = EncodingTable[index];
            bitCount -= 5;
            outIndex++;
            input_consumed++;
         }
         
      }
      
      return outIndex;
   }
   
   size_t zbase32_encode_estimate_size(size_t data_size)
   {
      return static_cast<size_t>(ceilf(data_size * 8.0f / 5.0f));
   }
   
   std::string decode(const std::string& data)
   {
      if (data.empty()){
         return std::string();
      }
      
      for (size_t i= 0; i < EncodingTable.size(); ++i) {
         DecodingTable.at(static_cast<size_t>(EncodingTable[i])) = static_cast<unsigned char>(i);
      }
      
      std::vector<unsigned char> result = std::vector<unsigned char>(static_cast<size_t>(ceilf(data.length() * 5.0f / 8.0f)));
      
      int index[8] = {0};
      for (size_t i = 0; i < data.length();) {
         i = createIndex(data, i, index);
         
         int shortByteCount = 0;
         unsigned long long buffer = 0;
         
         for (int j = 0; j < 8 && index[j] != -1; ++j) {
            buffer = (buffer << 5) | (unsigned long long)(DecodingTable[index[j]] & 0x1f);
            ++shortByteCount;
         }
         
         int bitCount = shortByteCount * 5;
         
         while (bitCount >= 8) {
            unsigned char push = (unsigned char)((buffer >> (bitCount - 8)) & 0xff);
            result.push_back(push);
            bitCount -= 8;
         }
      }
      return std::string((char*)result.data(), result.size());
   }
} }
