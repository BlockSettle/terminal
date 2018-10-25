#ifndef __WALLET_KEY_WIDGET_H__
#define __WALLET_KEY_WIDGET_H__

#include <QTimer>
#include <QWidget>
#include <botan/ecdh.h>
#include <botan/pubkey.h>
#include "EncryptionUtils.h"
#include "MobileClientRequestType.h"

namespace Ui {
    class WalletKeyWidget;
}
class QPropertyAnimation;
class MobileClient;
class ApplicationSettings;

class WalletKeyWidget : public QWidget
{
   Q_OBJECT
public:
   WalletKeyWidget(MobileClientRequest requestType, const std::string &walletId
      , int index, bool password
      , const std::pair<Botan::ECDH_PrivateKey, Botan::ECDH_PublicKey> &
      , QWidget* parent = nullptr);
   ~WalletKeyWidget() override;

   void init(const std::shared_ptr<ApplicationSettings>& appSettings, const QString& username);
   void cancel();
   void start();

   void setEncryptionKeys(const std::vector<SecureBinaryData> &encKeys, int index = 0);
   void setFixedType(bool on = true);
   void setFocus();

   void setHideAuthConnect(bool value);
   void setHideAuthCombobox(bool value);
   void setProgressBarFixed(bool value);
   void setShowAuthId(bool value);
   void setShowAuthIdLabel(bool value);
   void setPasswordLabelAsNew();
   void setPasswordLabelAsOld();
   void setHideAuthEmailLabel(bool value);
   void setHideAuthControlsOnSignClicked(bool value);
   const std::string &deviceId() const;

signals:
   void keyChanged(int index, SecureBinaryData);
   void encKeyChanged(int index, SecureBinaryData);
   void keyTypeChanged(int index, bool password);
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
   std::unique_ptr<Ui::WalletKeyWidget> ui_;
   std::string walletId_;
   int         index_;
   bool        password_;
   bool        authRunning_ = false;
   bool        encryptionKeysSet_ = false;

   QTimer      timer_;
   float       timeLeft_;
//   QString     prompt_;
   MobileClient *mobileClient_{};

   bool        hideAuthConnect_ = false;
   bool        hideAuthCombobox_ = false;
   bool        progressBarFixed_ = false;
   bool        showAuthId_ = false;
   bool        hideAuthEmailLabel_ = false;
   bool        hideAuthControlsOnSignClicked_ = false;
   MobileClientRequest requestType_{};
   std::string deviceId_;
};

#endif // __WALLET_KEY_WIDGET_H__
