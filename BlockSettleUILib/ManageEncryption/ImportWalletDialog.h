#ifndef __IMPORT_WALLET_DIALOG_H__
#define __IMPORT_WALLET_DIALOG_H__

#include <memory>
#include <QDialog>

#include "BtcDefinitions.h"
#include "EasyCoDec.h"
#include "CoreWallet.h"
#include "QWalletInfo.h"

namespace Ui {
   class ImportWalletDialog;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class ApplicationSettings;
class ArmoryObject;
class AssetManager;
class AuthAddressManager;
class SignContainer;
class WalletImporter;
class ConnectionManager;

class ImportWalletDialog : public QDialog
{
Q_OBJECT

public:
   ImportWalletDialog(const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<ArmoryObject> &
      , const EasyCoDec::Data& walletData
      , const EasyCoDec::Data& chainCodeData
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , const std::shared_ptr<spdlog::logger> &
      , const QString& username
      , const std::string &walletName = {}
      , const std::string &walletDesc = {}
      , bool disableImportPrimary = false
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
   void updateAcceptButtonState();
   void onWalletInfo(unsigned int reqId, const bs::hd::WalletInfo &walletInfo);
   void onSignerError(unsigned int id, std::string error);
   void promptForSignWalletDelete();
protected:
   void reject() override;

private:
   std::unique_ptr<Ui::ImportWalletDialog>   ui_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<SignContainer>   signContainer_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<ArmoryObject>    armory_;
   std::shared_ptr<WalletImporter>  walletImporter_;
   std::shared_ptr<spdlog::logger> logger_;
   bs::core::wallet::Seed  walletSeed_;
   bs::hd::WalletInfo walletInfo_;

   bool importedAsPrimary_ = false;
   bool authNoticeWasShown_ = false;
   bool existingChecked_ = false;
   unsigned int reqWalletInfoId_ = 0;
   bool disableImportPrimary_;
};

#endif // __IMPORT_WALLET_DIALOG_H__
