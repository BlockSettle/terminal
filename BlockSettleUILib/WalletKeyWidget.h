#ifndef __WALLET_KEY_WIDGET_H__
#define __WALLET_KEY_WIDGET_H__

#include <QTimer>
#include <QWidget>
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
      , int index, bool password, QWidget* parent = nullptr);
   ~WalletKeyWidget() override;

   void init(const std::shared_ptr<ApplicationSettings>& appSettings, const QString& username);
   void cancel();
   void start();

   void setEncryptionKeys(const std::vector<SecureBinaryData> &encKeys, int index = 0);
   void setFixedType(bool on = true);
   void setFocus();

   void setHideFrejaConnect(bool value);
   void setHideFrejaCombobox(bool value);
   void setProgressBarFixed(bool value);
   void setShowFrejaId(bool value);
   void setShowFrejaIdLabel(bool value);
   void setPasswordLabelAsNew();
   void setPasswordLabelAsOld();
   void setHideFrejaEmailLabel(bool value);
   void setHideFrejaControlsOnSignClicked(bool value);

signals:
   void keyChanged(int index, SecureBinaryData);
   void encKeyChanged(int index, SecureBinaryData);
   void keyTypeChanged(int index, bool password);
   // Signals that Freja was denied or timed out
   void failed();

private slots:
   void onTypeChanged();
   void onPasswordChanged();
   void onFrejaIdChanged(const QString &);
   void onFrejaSignClicked();
   void onFrejaSucceeded(const SecureBinaryData &password);
   void onFrejaFailed(const QString &text);
   void onFrejaStatusUpdated(const QString &status);
   void onTimer();

private:
   void stop();
   QPropertyAnimation* startFrejaAnimation(bool success);

private:
   std::unique_ptr<Ui::WalletKeyWidget> ui_;
   std::string walletId_;
   int         index_;
   bool        password_;
   bool        frejaRunning_ = false;
   bool        encryptionKeysSet_ = false;

//   FrejaSignWallet frejaSign_;
   QTimer      timer_;
   float       timeLeft_;
//   QString     prompt_;
   MobileClient *mobileClient_{};

   bool        hideFrejaConnect_ = false;
   bool        hideFrejaCombobox_ = false;
   bool        progressBarFixed_ = false;
   bool        showFrejaId_ = false;
   bool        hideFrejaEmailLabel_ = false;
   bool        hideFrejaControlsOnSignClicked_ = false;
   MobileClientRequest requestType_{};
};

#endif // __WALLET_KEY_WIDGET_H__
