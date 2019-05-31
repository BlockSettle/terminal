#ifndef __WALLET_BACKUP_FILE_H__
#define __WALLET_BACKUP_FILE_H__

#include <memory>
#include <string>

#include "EasyCoDec.h"

namespace bs {
   namespace core {
      namespace hd {
         class Wallet;
      }
   }
}

class WalletBackupFile
{
public:
   struct WalletData {
      std::string id;
      std::string name;
      std::string description;
      EasyCoDec::Data   seed;
      EasyCoDec::Data   chainCode;
   };

   WalletBackupFile(const std::string &id, const std::string &name
      , const std::string &description, const EasyCoDec::Data& seedData
      , const std::string& privKey);
   ~WalletBackupFile() noexcept  = default;

   static WalletData Deserialize(const std::string& rawData);
   std::string       Serialize() const;

private:
   std::string id_;
   std::string name_;
   std::string description_;
   EasyCoDec::Data seedData_;
   std::string privKey_;
};

#endif // __WALLET_BACKUP_FILE_H__
