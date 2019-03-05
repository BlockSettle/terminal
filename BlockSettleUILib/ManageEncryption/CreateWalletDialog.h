#ifndef __CREATE_WALLET_DIALOG_H__
#define __CREATE_WALLET_DIALOG_H__

#include <memory>
#include <vector>
#include <QDialog>
#include <QValidator>
#include "BtcDefinitions.h"
#include "EncryptionUtils.h"
#include "MetaData.h"
#include "QWalletInfo.h"

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
class ConnectionManager;

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
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , const std::shared_ptr<spdlog::logger> &logger
      , QWidget *parent = nullptr);
   ~CreateWalletDialog() override;

   bool walletCreated() const { return walletCreated_; }
   bool isNewWalletPrimary() const { return createdAsPrimary_; }

private slots:
   void updateAcceptButtonState();
   void createWallet();
   void onWalletCreated(unsigned int id, std::shared_ptr<bs::hd::Wallet>);
   void onWalletCreateError(unsigned int id, std::string errMsg);

protected:
   void reject() override;

private:
   std::unique_ptr<Ui::CreateWalletDialog> ui_;

private:
   std::shared_ptr<WalletsManager>  walletsManager_;
   std::shared_ptr<SignContainer>   signingContainer_;
   const std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager>           connectionManager_;
   std::shared_ptr<spdlog::logger> logger_;
   const QString     walletsPath_;
   const bs::wallet::Seed walletSeed_;
   bs::hd::WalletInfo walletInfo_;
   unsigned int      createReqId_ = 0;
   bool              walletCreated_ = false;
   bool              createdAsPrimary_ = false;
   bool              authNoticeWasShown_ = false;
};

#endif // __CREATE_WALLET_DIALOG_H__
