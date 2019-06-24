#include "UserHasher.h"
#include "ZBase32.h"
namespace {
   const std::string kdf_iv = "2b"                //total_length
                              "00c1"              //Romix_KDF
                              "01000000"          //Iterations
                              "00040000"          //memory_target
                              "20"                //salt_length
                              "0000000000000000"  //salt
                              "0000000000000000"  //salt+8
                              "0000000000000000"  //salt+16
                              "0000000000000000"; //salt+24
}

const unsigned int UserHasher::KeyLength = 12;

UserHasher::UserHasher(const BinaryData& iv)
   : iv_(iv)
{
   if (iv_.isNull()){
      iv_ = SecureBinaryData::CreateFromHex(kdf_iv);
   }
}

std::shared_ptr<KeyDerivationFunction> UserHasher::getKDF()
{
   if (!iv_.isNull() && !kdf_) {
      try {
         kdf_ = KeyDerivationFunction::deserialize(iv_.getRef());
      } catch (const std::exception &) {
         iv_.clear();
      }
   }
   if (!kdf_) {
      kdf_ = std::make_shared<KeyDerivationFunction_Romix>();
      if (iv_.isNull ()) {
         iv_ = kdf_->serialize();
      }
   }
   return kdf_;
}

std::string UserHasher::deriveKey(const std::string& rawData)
{
   SecureBinaryData key = getKDF()->deriveKey(SecureBinaryData(rawData));
   std::vector<uint8_t> keyData(key.getPtr(), key.getPtr() + key.getSize());
   return bs::zbase32Encode(keyData).substr(0, KeyLength);
}
