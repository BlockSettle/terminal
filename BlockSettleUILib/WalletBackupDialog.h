#ifndef __WALLET_BACKUP_DIALOG_H__
#define __WALLET_BACKUP_DIALOG_H__

#include <QDialog>
#include <memory>
#include "EncryptionUtils.h"
#include "FrejaREST.h"
#include "HDNode.h"
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
   void onPasswordChanged();
   void onSelectFile();
   void onRootKeyReceived(unsigned int id, const SecureBinaryData &privKey, const SecureBinaryData &chainCode
      , std::string walletId);
   void onHDWalletInfo(unsigned int id, std::vector<bs::wallet::EncryptionType>, std::vector<SecureBinaryData> encKeys
      , bs::hd::KeyRank);
   void onContainerError(unsigned int id, std::string errMsg);
   void showError(const QString &title, const QString &text);

   void startFrejaSign();
   void onFrejaSucceeded(SecureBinaryData);
   void onFrejaFailed(const QString &text);
   void onFrejaStatusUpdated(const QString &status);

private:
   Ui::WalletBackupDialog *ui_;
   std::shared_ptr<bs::hd::Wallet>     wallet_;
   std::shared_ptr<SignContainer>      signingContainer_;
   SecureBinaryData                    walletPassword_;
   std::vector<bs::wallet::EncryptionType>   walletEncTypes_;
   std::vector<SecureBinaryData>             walletEncKeys_;
   bs::hd::KeyRank   walletEncRank_;
   FrejaSignWallet   frejaSign_;
   unsigned int   infoReqId_ = 0;
   unsigned int   privKeyReqId_ = 0;
   std::string    outputFile_;
   std::string    outputDir_;
   QString        selectedFile_;
};

#endif // __WALLET_BACKUP_DIALOG_H__
