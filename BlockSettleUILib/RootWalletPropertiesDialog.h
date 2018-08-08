#ifndef __ROOT_WALLET_PROPERTIES_DIALOG_H__
#define __ROOT_WALLET_PROPERTIES_DIALOG_H__

#include <QDialog>
#include <memory>
#include "BinaryData.h"
#include "MetaData.h"

namespace Ui {
    class WalletPropertiesDialog;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
   class Wallet;
}
class ArmoryConnection;
class ApplicationSettings;
class AssetManager;
class CurrentWalletFilter;
class SignContainer;
class WalletsManager;
class WalletsViewModel;

class RootWalletPropertiesDialog : public QDialog
{
Q_OBJECT

public:
   RootWalletPropertiesDialog(const std::shared_ptr<bs::hd::Wallet> &, const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<ArmoryConnection> &, const std::shared_ptr<SignContainer> &
      , WalletsViewModel *walletsModel, const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<AssetManager> &, QWidget* parent = nullptr);
   ~RootWalletPropertiesDialog() override = default;

private slots:
   void onDeleteWallet();
   void onBackupWallet();
   void onCreateWoWallet();
   void onChangePassword();
   void onPasswordChanged(const std::string &walletId, bool ok);
   void onHDWalletInfo(unsigned int id, bs::wallet::EncryptionType, const SecureBinaryData &encKey);
   void onWalletSelected();
   void onRescanBlockchain();
   void onHDLeafCreated(unsigned int id, BinaryData pubKey, BinaryData chainCode, std::string walletId);
   void onModelReset();

private:
   void copyWoWallet();

   void updateWalletDetails(const std::shared_ptr<bs::hd::Wallet>& wallet);
   void updateWalletDetails(const std::shared_ptr<bs::Wallet>& wallet);
   void startWalletScan();

private:
   Ui::WalletPropertiesDialog    *     ui_;
   std::shared_ptr<bs::hd::Wallet>     wallet_;
   std::shared_ptr<WalletsManager>     walletsManager_;
   std::shared_ptr<SignContainer>      signingContainer_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<AssetManager>       assetMgr_;
   CurrentWalletFilter                 *walletFilter_;
   unsigned int                        infoReqId_ = 0;
   bs::wallet::EncryptionType          walletEncType_ = bs::wallet::EncryptionType::Password;
   SecureBinaryData                    walletEncKey_;
   std::map<unsigned int, std::string> createCCWalletReqs_;
};

#endif // __WALLET_PROPERTIES_DIALOG_H__
