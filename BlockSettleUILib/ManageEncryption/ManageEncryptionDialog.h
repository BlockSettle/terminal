#ifndef __MANAGE_ENCRYPTION_DIALOG_H__
#define __MANAGE_ENCRYPTION_DIALOG_H__

#include <memory>
#include <QDialog>
#include "EncryptionUtils.h"
#include "AutheIDClient.h"
#include "QWalletInfo.h"

namespace Ui {
    class ManageEncryptionDialog;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Wallet;
      }
   }
}

class WalletKeyWidget;
class ApplicationSettings;
class SignContainer;

class ManageEncryptionDialog : public QDialog
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

   ManageEncryptionDialog(const std::shared_ptr<spdlog::logger> &logger
      , std::shared_ptr<SignContainer> signingContainer
      , const std::shared_ptr<bs::sync::hd::Wallet> &wallet
      , const bs::hd::WalletInfo &walletInfo
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , QWidget* parent = nullptr);
   ~ManageEncryptionDialog() override;

private slots:
   void onContinueClicked();
   void onTabChanged(int index);

   void onPasswordChanged(const std::string &walletId, bool ok);

protected:
   void accept() override;

private:
   void updateState();
   void continueBasic();
   void continueAddDevice();
   void changePassword();
   void resetKeys();
   void deleteDevice(const std::string &deviceId);

   std::unique_ptr<Ui::ManageEncryptionDialog> ui_;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<SignContainer> signingContainer_;
   std::shared_ptr<bs::sync::hd::Wallet>  wallet_;
   bs::wallet::KeyRank newKeyRank_;
   std::vector<bs::wallet::PasswordData> oldPasswordData_;
   std::vector<bs::wallet::PasswordData> newPasswordData_;

   bool addNew_;
   bool removeOld_;
   SecureBinaryData oldKey_;
   State state_ = State::Idle;
   bool deviceKeyOldValid_;
   bool deviceKeyNewValid_;
   bool isLatestChangeAddDevice_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager>   connectionManager_;
   bs::hd::WalletInfo walletInfo_;
};

#endif // __MANAGE_ENCRYPTION_DIALOG_H__
