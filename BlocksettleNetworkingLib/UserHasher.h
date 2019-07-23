#ifndef __USER_HASHER_H__
#define __USER_HASHER_H__

#include <memory>
#include <string>

class BinaryData;
class KeyDerivationFunction;

class UserHasher {
public:
   static const int KeyLength;

   UserHasher();
   UserHasher(const BinaryData& iv);

   std::shared_ptr<KeyDerivationFunction> getKDF() const { return kdf_; }

   std::string deriveKey(const std::string& rawData) const;
private:
   std::shared_ptr<KeyDerivationFunction> kdf_;
};

using UserHasherPtr = std::shared_ptr<UserHasher>;

#endif //__USER_HASHER_H__
