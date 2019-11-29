/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "WalletBackupFile.h"
#include "CoreHDWallet.h"

#include "bs_storage.pb.h"

WalletBackupFile::WalletBackupFile(const std::string &id, const std::string &name
   , const std::string &description, const EasyCoDec::Data& data
   , const std::string &privKey)
  : id_(id), name_(name), description_(description)
  , seedData_(data), privKey_(privKey)
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

   backup.set_id(id_);
   backup.set_name(name_);

   if (!description_.empty()) {
      backup.set_description(description_);
   }
   backup.set_seed1(seedData_.part1);
   backup.set_seed2(seedData_.part2);

   backup.set_chaincode1(privKey_);
   backup.set_chaincode2("");

   return backup.SerializeAsString();
}
