#ifndef __CREATE_WALLET_DIALOG_H__
#define __CREATE_WALLET_DIALOG_H__

#include <memory>
#include <vector>
#include <QDialog>
#include <QValidator>
#include "BtcDefinitions.h"
#include "EncryptionUtils.h"
#include "MetaData.h"


namespace Ui {
   class CreateWalletDialog;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
}
class SignContainer;
class WalletsManager;
class WalletKeysCreateWidget;
class ApplicationSettings;

class CreateWalletDialog : public QDialog
{
   Q_OBJECT

public:
   // Username is used to init Auth ID when available
   CreateWalletDialog(const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , const QString &walletsPath
      , const bs::wallet::Seed& walletSeed
      , const std::string& walletId
      , const QString& username
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , QWidget *parent = nullptr);
   ~CreateWalletDialog() override;

   bool walletCreated() const { return walletCreated_; }
   bool isNewWalletPrimary() const { return createdAsPrimary_; }

private slots:
   void updateAcceptButtonState();
   void CreateWallet();
   void onWalletCreated(unsigned int id, std::shared_ptr<bs::hd::Wallet>);
   void onWalletCreateError(unsigned int id, std::string errMsg);
   void onKeyTypeChanged(bool password);

protected:
   void reject() override;

private:
   std::unique_ptr<Ui::CreateWalletDialog> ui_;

private:
   std::shared_ptr<WalletsManager>  walletsManager_;
   std::shared_ptr<SignContainer>   signingContainer_;
   const std::shared_ptr<ApplicationSettings> appSettings_;
   const QString     walletsPath_;
   const bs::wallet::Seed walletSeed_;
   const std::string walletId_;
   unsigned int      createReqId_ = 0;
   bool              walletCreated_ = false;
   SecureBinaryData  walletPassword_;
   bool              createdAsPrimary_ = false;
   bool              authNoticeWasShown_ = false;
};

// Common function for CreateWalletDialog and ImportWalletDialog.
// Checks validity and returns updated keys in keys output argument if succeeds.
// Shows error messages if needed.
bool checkNewWalletValidity(WalletsManager* walletsManager
   , const QString& walletName
   , const std::string& walletId
   , WalletKeysCreateWidget* widgetCreateKeys
   , std::vector<bs::wallet::PasswordData>* keys
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , QWidget* parent);

#endif // __CREATE_WALLET_DIALOG_H__
