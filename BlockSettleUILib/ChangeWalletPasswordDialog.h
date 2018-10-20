#ifndef __CHANGE_WALLET_PASSWORD_DIALOG_H__
#define __CHANGE_WALLET_PASSWORD_DIALOG_H__

#include <memory>
#include <QDialog>
#include "EncryptionUtils.h"
#include "MetaData.h"

namespace Ui {
    class ChangeWalletPasswordDialog;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
}

class WalletKeyWidget;
class ApplicationSettings;
class SignContainer;

class ChangeWalletPasswordDialog : public QDialog
{
Q_OBJECT

public:
   enum class Pages {
      Basic,
      AddDevice,
   };

   enum class State {
      Idle,
      AddDeviceWaitOld,
      AddDeviceWaitNew,
   };

   ChangeWalletPasswordDialog(std::shared_ptr<SignContainer> signingContainer
      , const std::shared_ptr<bs::hd::Wallet> &, const std::vector<bs::wallet::EncryptionType> &
      , const std::vector<SecureBinaryData> &encKeys
      , bs::wallet::KeyRank
      , const QString &username
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , QWidget* parent = nullptr);
   ~ChangeWalletPasswordDialog() override;

   SecureBinaryData oldPassword() const;
   std::vector<bs::wallet::PasswordData> newPasswordData() const;
   bs::wallet::KeyRank newKeyRank() const;

private slots:
   void onContinueClicked();
   void onTabChanged(int index);
   void onSubmitKeysKeyChanged2(int, SecureBinaryData);
   void onSubmitKeysFailed2();
   void onCreateKeysKeyChanged2(int, SecureBinaryData);
   void onCreateKeysFailed2();
   void onPasswordChanged(const std::string &walletId, bool ok);

protected:
   void accept() override;
   void reject() override;

private:
   void updateState();
   void continueBasic();
   void continueAddDevice();

   std::unique_ptr<Ui::ChangeWalletPasswordDialog> ui_;
   std::shared_ptr<SignContainer> signingContainer_;
   std::shared_ptr<bs::hd::Wallet>  wallet_;
   const bs::wallet::KeyRank oldKeyRank_;
   bs::wallet::KeyRank newKeyRank_;
   std::vector<bs::wallet::PasswordData> oldPasswordData_;
   std::vector<bs::wallet::PasswordData> newPasswordData_;
   SecureBinaryData oldKey_;
   State state_ = State::Idle;
   WalletKeyWidget *deviceKeyOld_ = nullptr;
   WalletKeyWidget *deviceKeyNew_ = nullptr;
   bool deviceKeyOldValid_ = false;
   bool deviceKeyNewValid_ = false;
   bool isLatestChangeAddDevice_ = false;
   std::shared_ptr<ApplicationSettings> appSettings_;
};

#endif // __CHANGE_WALLET_PASSWORD_DIALOG_H__
