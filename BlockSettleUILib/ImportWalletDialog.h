#ifndef __IMPORT_WALLET_DIALOG_H__
#define __IMPORT_WALLET_DIALOG_H__

#include <memory>
#include <QDialog>

#include "BtcDefinitions.h"
#include "EasyCoDec.h"
#include "MetaData.h"
#include "QWalletInfo.h"

namespace Ui {
   class ImportWalletDialog;
}
class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class SignContainer;
class WalletImporter;
class WalletsManager;

class ImportWalletDialog : public QDialog
{
Q_OBJECT

public:
   ImportWalletDialog(const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<ArmoryConnection> &
      , const EasyCoDec::Data& walletData
      , const EasyCoDec::Data& chainCodeData
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<spdlog::logger> &logger
      , const QString& username
      , const std::string &walletName = {}
      , const std::string &walletDesc = {}
      , QWidget *parent = nullptr);
   ~ImportWalletDialog() override;

   QString getNewWalletName() const { return walletInfo_.name(); }
   std::string getWalletId() const { return walletInfo_.rootId().toStdString(); }
   std::shared_ptr<WalletImporter> getWalletImporter() const { return walletImporter_; }

   bool ImportedAsPrimary() const { return importedAsPrimary_; }

private slots:
   void importWallet();
   void onWalletCreated(const std::string &walletId);
   void onError(const QString &errMsg);
   void onKeyTypeChanged(bool password);
   void updateAcceptButtonState();

protected:
   void reject() override;

private:
   std::unique_ptr<Ui::ImportWalletDialog> ui_;
   std::shared_ptr<WalletsManager>  walletsManager_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::shared_ptr<WalletImporter>  walletImporter_;
   std::shared_ptr<spdlog::logger> logger_;
   bs::wallet::Seed  walletSeed_;
   bs::hd::WalletInfo walletInfo_;

   bool importedAsPrimary_ = false;
   bool authNoticeWasShown_ = false;
};

#endif // __IMPORT_WALLET_DIALOG_H__
