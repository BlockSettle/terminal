#ifndef __WALLET_KEYS_SUBMIT_WIDGET_H__
#define __WALLET_KEYS_SUBMIT_WIDGET_H__

#include <QWidget>
#include "WalletEncryption.h"
#include "AutheIDClient.h"

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
      , const std::string &walletId
      , bs::wallet::KeyRank
      , const std::vector<bs::wallet::EncryptionType> &
      , const std::vector<SecureBinaryData> &encKeys
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const QString &prompt = QString());
   void cancel();

   bool isValid() const;
   std::string encKey(int index) const;
   SecureBinaryData key() const;
   bool isKeyFinal() const;

   void setFocus();
   void suspend() { suspended_ = true; }
   void resume();

signals:
   void keyChanged();
   void keyCountChanged();
   void failed();

private slots:
   void onKeyChanged(int index, SecureBinaryData);
   void onKeyTypeChanged(int index, bool password);
   void onEncKeyChanged(int index, SecureBinaryData);

private:
   void addKey(bool password, const std::vector<SecureBinaryData> &encKeys
      , int encKeyIndex = 0, bool isFixed = false, const QString &prompt = QString());

private:
   std::unique_ptr<Ui::WalletKeysSubmitWidget> ui_;
   std::string walletId_;
   std::vector<WalletKeyWidget *> widgets_;
   std::vector<bs::wallet::PasswordData> pwdData_;
   std::atomic_bool suspended_;
   Flags flags_{NoFlag};
   std::shared_ptr<ApplicationSettings> appSettings_;
   AutheIDClient::RequestType requestType_{};
   bool isKeyFinal_{false};
};

Q_DECLARE_OPERATORS_FOR_FLAGS(WalletKeysSubmitWidget::Flags)

#endif // __WALLET_KEYS_SUBMIT_WIDGET_H__
