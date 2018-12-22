#ifndef __CHANGE_WALLET_PASSWORD_DIALOG_H__
#define __CHANGE_WALLET_PASSWORD_DIALOG_H__

#include <memory>
#include <QDialog>
#include "EncryptionUtils.h"
#include "MetaData.h"
#include "AutheIDClient.h"

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

   ChangeWalletPasswordDialog(const std::shared_ptr<spdlog::logger> &logger
      , std::shared_ptr<SignContainer> signingContainer
      , const std::shared_ptr<bs::hd::Wallet> &, const std::vector<bs::wallet::EncryptionType> &
      , const std::vector<SecureBinaryData> &encKeys
      , bs::wallet::KeyRank
      , const QString &username
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , QWidget* parent = nullptr);
   ~ChangeWalletPasswordDialog() override;

private slots:
   void onContinueClicked();
   void onTabChanged(int index);

   void onOldDeviceKeyChanged(int, SecureBinaryData);
   void onOldDeviceFailed();

   void onNewDeviceEncKeyChanged(int index, SecureBinaryData encKey);
   void onNewDeviceKeyChanged(int index, SecureBinaryData password);
   void onNewDeviceFailed();

   void onPasswordChanged(const std::string &walletId, bool ok);

protected:
   void accept() override;
   void reject() override;

private:
   void updateState();
   void continueBasic();
   void continueAddDevice();
   void changePassword();
   void resetKeys();
   void deleteDevice(const std::string &deviceId);

   std::unique_ptr<Ui::ChangeWalletPasswordDialog> ui_;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<SignContainer> signingContainer_;
   std::shared_ptr<bs::hd::Wallet>  wallet_;
   const bs::wallet::KeyRank oldKeyRank_;
   bs::wallet::KeyRank newKeyRank_;
   std::vector<bs::wallet::PasswordData> oldPasswordData_;
   std::vector<bs::wallet::PasswordData> newPasswordData_;
   // Init variables in resetKeys method so they always valid when we restart process
   bool addNew_;
   bool removeOld_;
   SecureBinaryData oldKey_;
   State state_ = State::Idle;
   WalletKeyWidget *deviceKeyOld_ = nullptr;
   WalletKeyWidget *deviceKeyNew_ = nullptr;
   bool deviceKeyOldValid_;
   bool deviceKeyNewValid_;
   bool isLatestChangeAddDevice_;
   std::shared_ptr<ApplicationSettings> appSettings_;
};

#endif // __CHANGE_WALLET_PASSWORD_DIALOG_H__
