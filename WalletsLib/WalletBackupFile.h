#ifndef __WALLET_BACKUP_FILE_H__
#define __WALLET_BACKUP_FILE_H__

#include <memory>
#include <string>

#include "EasyCoDec.h"

namespace bs {
   namespace hd {
      class Wallet;
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

   WalletBackupFile(const std::shared_ptr<bs::hd::Wallet> &
      , const EasyCoDec::Data& data
      , const EasyCoDec::Data& chainCode);
   ~WalletBackupFile() noexcept  = default;

   static WalletData Deserialize(const std::string& rawData);
   std::string       Serialize() const;

private:
   const std::shared_ptr<bs::hd::Wallet>  wallet_;
   EasyCoDec::Data data_;
   EasyCoDec::Data chainCode_;
};

#endif // __WALLET_BACKUP_FILE_H__
