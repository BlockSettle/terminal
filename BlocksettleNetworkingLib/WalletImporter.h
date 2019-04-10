#ifndef __WALLET_IMPORTER_H__
#define __WALLET_IMPORTER_H__

#include <functional>
#include <memory>
#include <string>
#include <QObject>
#include <QStringList>
#include "CoreWallet.h"
#include "HDPath.h"
#include "WalletEncryption.h"

namespace bs {
   namespace sync {
      namespace hd {
         class Group;
         class Leaf;
         class Wallet;
      }
      class WalletsManager;
   }
}
class ApplicationSettings;
class ArmoryObject;
class AssetManager;
class AuthAddressManager;
class SignContainer;


class WalletImporter : public QObject
{
   Q_OBJECT

public:
   using CbScanReadLast = std::function<unsigned int(const std::string &walletId)>;
   using CbScanWriteLast = std::function<void(const std::string &walletId, unsigned int idx)>;

   WalletImporter(const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<ArmoryObject> &, const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<AuthAddressManager> &
      , const CbScanReadLast &, const CbScanWriteLast &);

   void Import(const std::string& name, const std::string& description
      , bs::core::wallet::Seed seed, bool primary = false
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}, bs::wallet::KeyRank keyRank = { 0, 0 });

signals:
   void walletCreated(const std::string &rootWalletId);
   void error(const QString &errMsg);

private slots:
   void onHDWalletCreated(unsigned int id, std::shared_ptr<bs::sync::hd::Wallet>);
   void onHDLeafCreated(unsigned int id, const std::shared_ptr<bs::sync::hd::Leaf> &);
   void onHDWalletError(unsigned int id, std::string error);
   void onWalletScanComplete(bs::sync::hd::Group *, bs::hd::Path::Elem wallet, bool isValid);

private:
   std::shared_ptr<SignContainer>      signingContainer_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<ArmoryObject>       armory_;
   std::shared_ptr<AssetManager>       assetMgr_;
   std::shared_ptr<AuthAddressManager> authMgr_;
   const CbScanReadLast    cbReadLast_;
   const CbScanWriteLast   cbWriteLast_;
   std::shared_ptr<bs::sync::hd::Wallet>     rootWallet_;
   std::map<unsigned int, std::string> createCCWalletReqs_;
   unsigned int      createWalletReq_ = 0;
   std::vector<bs::wallet::PasswordData>  pwdData_;
   bs::wallet::KeyRank  keyRank_;
   std::unordered_map<unsigned int, bs::hd::Path>     createNextWalletReqs_;
};

#endif // __WALLET_IMPORTER_H__
