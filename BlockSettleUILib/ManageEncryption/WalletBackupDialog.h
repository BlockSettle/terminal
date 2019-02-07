#ifndef __WALLET_BACKUP_DIALOG_H__
#define __WALLET_BACKUP_DIALOG_H__

#include <QDialog>
#include <memory>
#include "EncryptionUtils.h"
#include "MetaData.h"
#include "QWalletInfo.h"


namespace Ui {
   class WalletBackupDialog;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
}
class SignContainer;
class WalletsManager;
class ApplicationSettings;


class WalletBackupDialog : public QDialog
{
   Q_OBJECT

public:
   WalletBackupDialog(const std::shared_ptr<bs::hd::Wallet> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<spdlog::logger> &logger
      , QWidget *parent = nullptr);
   ~WalletBackupDialog() override;

   bool isDigitalBackup() const;
   QString filePath() const;

private slots:
   void reject() override;
   void textFileClicked();
   void pdfFileClicked();
   void onBackupClicked();
   void onSelectFile();
   void onRootKeyReceived(unsigned int id, const SecureBinaryData &privKey, const SecureBinaryData &chainCode
      , std::string walletId);
   void onWalletInfo(unsigned int id, const bs::hd::WalletInfo &walletInfo);
   void onContainerError(unsigned int id, std::string errMsg);
   void showError(const QString &title, const QString &text);

private:
   std::unique_ptr<Ui::WalletBackupDialog> ui_;
   std::shared_ptr<bs::hd::Wallet>     wallet_;
   std::shared_ptr<SignContainer>      signingContainer_;
   unsigned int   infoReqId_ = 0;
   unsigned int   privKeyReqId_ = 0;
   std::string    outputFile_;
   std::string    outputDir_;
   QString        selectedFile_;
   const std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<spdlog::logger> logger_;
   bs::hd::WalletInfo walletInfo_;
};

bool WalletBackupAndNewVerify(const std::shared_ptr<bs::hd::Wallet> &
   , const std::shared_ptr<SignContainer> &
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<spdlog::logger> &logger
   , QWidget *parent);

#endif // __WALLET_BACKUP_DIALOG_H__
