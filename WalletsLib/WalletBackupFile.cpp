#include "WalletBackupFile.h"
#include "HDWallet.h"

#include "bs_storage.pb.h"

WalletBackupFile::WalletBackupFile(const std::shared_ptr<bs::hd::Wallet> &wallet
   , const EasyCoDec::Data& data
   , const EasyCoDec::Data& chainCode)
  : wallet_(wallet)
  , data_(data)
  , chainCode_(chainCode)
{}

WalletBackupFile::WalletData WalletBackupFile::Deserialize(const std::string& rawData)
{
   WalletData result;
   Blocksettle::Storage::WalletBackupFile backup;
   if (!backup.ParseFromString(rawData)) {
      return result;
   }

   result.id = backup.id();
   if (backup.has_description()) {
      result.description = backup.description();
   }
   if (backup.has_name()) {
      result.name = backup.name();
   }

   result.seed.part1 = backup.seed1();
   result.seed.part2 = backup.seed2();

   result.chainCode.part1 = backup.chaincode1();
   result.chainCode.part2 = backup.chaincode2();

   return result;
}

std::string WalletBackupFile::Serialize() const
{
   Blocksettle::Storage::WalletBackupFile backup;

   backup.set_id(wallet_->getWalletId());
   backup.set_name(wallet_->getName());

   const auto &desc = wallet_->getDesc();
   if (!desc.empty()) {
      backup.set_description(desc);
   }
   backup.set_seed1(data_.part1);
   backup.set_seed2(data_.part2);

   backup.set_chaincode1(chainCode_.part1);
   backup.set_chaincode2(chainCode_.part2);

   return backup.SerializeAsString();
}
