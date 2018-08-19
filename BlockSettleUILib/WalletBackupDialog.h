#ifndef __WALLET_BACKUP_DIALOG_H__
#define __WALLET_BACKUP_DIALOG_H__

#include <QDialog>
#include <memory>
#include "EncryptionUtils.h"
#include "MetaData.h"


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


class WalletBackupDialog : public QDialog
{
   Q_OBJECT

public:
   WalletBackupDialog(const std::shared_ptr<bs::hd::Wallet> &, const std::shared_ptr<SignContainer> &
      , QWidget *parent = nullptr);
   ~WalletBackupDialog() noexcept override = default;

   bool isDigitalBackup() const;
   QString filePath() const;

private slots:
   void accept() override;
   void reject() override;
   void TextFileClicked();
   void PDFFileClicked();
   void onSelectFile();
   void onRootKeyReceived(unsigned int id, const SecureBinaryData &privKey, const SecureBinaryData &chainCode
      , std::string walletId);
   void onHDWalletInfo(unsigned int id, std::vector<bs::wallet::EncryptionType>, std::vector<SecureBinaryData> encKeys
      , bs::wallet::KeyRank);
   void onContainerError(unsigned int id, std::string errMsg);
   void showError(const QString &title, const QString &text);
   void updateState();

private:
   Ui::WalletBackupDialog *ui_;
   std::shared_ptr<bs::hd::Wallet>     wallet_;
   std::shared_ptr<SignContainer>      signingContainer_;
   unsigned int   infoReqId_ = 0;
   unsigned int   privKeyReqId_ = 0;
   std::string    outputFile_;
   std::string    outputDir_;
   QString        selectedFile_;
};

#endif // __WALLET_BACKUP_DIALOG_H__
