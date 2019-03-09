#ifndef __WALLET_KEYS_SUBMIT_WIDGET_H__
#define __WALLET_KEYS_SUBMIT_WIDGET_H__

#include <QWidget>
#include "WalletEncryption.h"
#include "AutheIDClient.h"
#include "QWalletInfo.h"
#include "WalletKeyWidget.h"

namespace Ui {
    class WalletKeysSubmitWidget;
}
class WalletKeyWidget;
class ApplicationSettings;

class WalletKeysSubmitWidget : public QWidget
{
   Q_OBJECT
public:
   enum Flag {
      NoFlag = 0x00,
      HideAuthConnectButton = 0x01,
      HideAuthCombobox = 0x02,
      HideGroupboxCaption = 0x04,
      AuthProgressBarFixed = 0x08,
      AuthIdVisible = 0x10,
      SetPasswordLabelAsOld = 0x20,
      HideAuthEmailLabel = 0x40,
      HideAuthControlsOnSignClicked = 0x80,
      HidePubKeyFingerprint = 0x100,
      HideProgressBar = 0x200,
      HidePasswordWarning = 0x400
   };
   Q_DECLARE_FLAGS(Flags, Flag)

   WalletKeysSubmitWidget(QWidget* parent = nullptr);
   ~WalletKeysSubmitWidget() override;

   void setFlags(Flags flags);

   void init(AutheIDClient::RequestType requestType
             , const bs::hd::WalletInfo &walletInfo
             , WalletKeyWidget::UseType useType
             , const std::shared_ptr<spdlog::logger> &logger
             , const std::shared_ptr<ApplicationSettings> &appSettings
             , const std::shared_ptr<ConnectionManager> &connectionManager
             , const QString &prompt = QString());

   void cancel();

   bool isValid() const;
   SecureBinaryData key() const;
   bool isKeyFinal() const;

   void suspend() { suspended_ = true; }
   void resume();

   bs::wallet::PasswordData passwordData(int keyIndex) const { return pwdData_.at(keyIndex); }
   std::vector<bs::wallet::PasswordData> passwordData() const { return pwdData_; }

signals:
   void keyChanged();
   void keyCountChanged();
   void failed();
   void returnPressed();

public slots:
   void setFocus();
   void onPasswordDataChanged(int index, bs::wallet::PasswordData passwordData);

private:
   void addKey(int encKeyIndex, const QString &prompt = QString());


private:
   std::unique_ptr<Ui::WalletKeysSubmitWidget> ui_;
   std::vector<WalletKeyWidget *> widgets_;
   std::vector<bs::wallet::PasswordData> pwdData_;
   std::atomic_bool suspended_;
   Flags flags_{NoFlag};
   AutheIDClient::RequestType requestType_{};
   bool isKeyFinal_{false};
   bs::hd::WalletInfo walletInfo_;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager> connectionManager_;
   WalletKeyWidget::UseType useType_{};
};

Q_DECLARE_OPERATORS_FOR_FLAGS(WalletKeysSubmitWidget::Flags)

#endif // __WALLET_KEYS_SUBMIT_WIDGET_H__
