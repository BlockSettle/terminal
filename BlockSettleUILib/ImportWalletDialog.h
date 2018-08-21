#ifndef __IMPORT_WALLET_DIALOG_H__
#define __IMPORT_WALLET_DIALOG_H__

#include <memory>
#include <QDialog>

#include "BtcDefinitions.h"
#include "EasyCoDec.h"
#include "FrejaREST.h"
#include "MetaData.h"

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
   ImportWalletDialog(const std::shared_ptr<WalletsManager> &, const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<AssetManager> &, const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<ArmoryConnection> &
      , const EasyCoDec::Data& walletData, const EasyCoDec::Data& chainCodeData
      , const std::shared_ptr<ApplicationSettings> &
      , const std::string &walletName = {}, const std::string &walletDesc = {}
      , bool createPrimary = false, QWidget *parent = nullptr);
   ~ImportWalletDialog() noexcept override = default;

   QString getNewWalletName() const { return walletName_; }
   std::string getWalletId() const { return walletId_; }
   std::shared_ptr<WalletImporter> getWalletImporter() const { return walletImporter_; }

   bool ImportedAsPrimary() const { return importedAsPrimary_; }

private slots:
   void updateAcceptButton();
   void onImportAccepted();
   void onWalletCreated(const std::string &walletId);
   void onError(const QString &errMsg);

protected:
   void reject() override;

private:
   bool couldImport() const;

private:
   Ui::ImportWalletDialog *   ui_;
   std::shared_ptr<WalletsManager>  walletsMgr_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::shared_ptr<WalletImporter>  walletImporter_;
   bs::wallet::Seed  walletSeed_;
   std::string walletId_;
   QString     walletName_;
   bool importedAsPrimary_ = false;
};

#endif // __IMPORT_WALLET_DIALOG_H__
