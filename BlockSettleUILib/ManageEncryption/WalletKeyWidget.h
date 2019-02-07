#ifndef __WALLET_KEY_WIDGET_H__
#define __WALLET_KEY_WIDGET_H__

#include <QTimer>
#include <QWidget>
#include "EncryptUtils.h"
#include "EncryptionUtils.h"
#include "AutheIDClient.h"
#include "QWalletInfo.h"

namespace Ui {
    class WalletKeyWidget;
}
class QPropertyAnimation;
class AutheIDClient;
class ApplicationSettings;

class WalletKeyWidget : public QWidget
{
   Q_OBJECT
public:
   WalletKeyWidget(AutheIDClient::RequestType requestType
                      , const bs::hd::WalletInfo &walletInfo
                      , int keyIndex
                      , const std::shared_ptr<ApplicationSettings>& appSettings
                      , const std::shared_ptr<spdlog::logger> &logger
                      , QWidget* parent = nullptr);

   enum class UseType {
      RequestAuthInParent,       // requests password or eid (depends of walletInfo) in parent widget
      RequestAuthAsDialog,       // requests password or eid (depends of walletInfo) in popup dialog
      ChangeAuthInParent,        // change password or eid (depends of user select) in parent widget
      ChangeToPasswordAsDialog,  // requests password to change as dialog (currently not used)
      ChangeToEidAsDialog,       // requests eid to change as dialog

      RequestAuthForDialog,      // just show only eid email to request auth in dialog (used in manage encryption only for eid)
      ChangeAuthForDialog        // requests password to change or email for eid (depends of user select) in parent widget
                                 // ChangeToEidAsDialog should be opened if eid selected
   };
   Q_ENUM(UseType)

   ~WalletKeyWidget() override = default;

   void cancel();
   void start();

   void setFocus();

   // initially WalletKeyWidget designed to embed it to another widgets, not for using as popup dialog
   // ChangeAuthAsDialog and RequestAuthAsDialog flags enables possibility to show popup dialog for authorization
   void setUseType(UseType useType);

signals:
   void returnPressed(int keyIndex);

   // emitted when password entered or eid auth recieved
   void passwordDataChanged(int keyIndex, const bs::wallet::PasswordData &passwordData);

   // Signals that Auth was denied or timed out
   void failed();

private slots:
   void onTypeChanged();
   void onPasswordChanged();
   void onAuthIdChanged(const QString &);
   void onAuthSignClicked();
   void onAuthSucceeded(const std::string &deviceId, const SecureBinaryData &password);
   void onAuthFailed(const QString &text);
   void onAuthStatusUpdated(const QString &status);
   void onTimer();

private:
   void stop();
   QPropertyAnimation* startAuthAnimation(bool success);

private:
   std::shared_ptr<ApplicationSettings> appSettings_;

   std::unique_ptr<Ui::WalletKeyWidget> ui_;
   int         keyIndex_;
   bool        authRunning_ = false;

   QTimer      timer_;
   float       timeLeft_;

   AutheIDClient *autheIDClient_{};
   AutheIDClient::RequestType requestType_{};
   std::vector<std::string> knownDeviceIds_; // contains only device id for key with index keyIndex

   bs::hd::WalletInfo walletInfo_;
   bs::wallet::PasswordData passwordData_;
   std::shared_ptr<spdlog::logger> logger_;
};

#endif // __WALLET_KEY_WIDGET_H__
