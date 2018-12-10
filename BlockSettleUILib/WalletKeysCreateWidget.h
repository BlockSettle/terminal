#ifndef __WALLET_KEYS_CREATE_WIDGET_H__
#define __WALLET_KEYS_CREATE_WIDGET_H__

#include <QWidget>
#include "WalletEncryption.h"
#include "MobileClient.h"

namespace Ui {
    class WalletKeysCreateWidget;
}
class WalletKeyWidget;
class ApplicationSettings;

class WalletKeysCreateWidget : public QWidget
{
   Q_OBJECT
public:
   enum Flag {
      NoFlag = 0x00,
      HideAuthConnectButton = 0x01,
      HideWidgetContol = 0x02,
      HideGroupboxCaption = 0x04,
      SetPasswordLabelAsNew = 0x08,
      HidePubKeyFingerprint = 0x10
   };
   Q_DECLARE_FLAGS(Flags, Flag)

   WalletKeysCreateWidget(QWidget* parent = nullptr);
   ~WalletKeysCreateWidget() override;

   void setFlags(Flags flags);
   void init(MobileClient::RequestType requestType
      , const std::string &walletId, const QString& username
      , const std::shared_ptr<ApplicationSettings>& appSettings);
   void addPasswordKey() { addKey(true); }
   void addAuthKey() { addKey(false); }
   void cancel();

   bool isValid() const;
   std::vector<bs::wallet::PasswordData> keys() const { return pwdData_; }
   bs::wallet::KeyRank keyRank() const { return keyRank_; }

signals:
   void keyChanged();
   void keyCountChanged();
   void failed();
   void keyTypeChanged(bool password);

private slots:
   void onAddClicked();
   void onDelClicked();
   void onKeyChanged(int index, SecureBinaryData);
   void onKeyTypeChanged(int index, bool password);
   void onEncKeyChanged(int index, SecureBinaryData);
   void updateKeyRank(int);

private:
   void addKey(bool password);

private:
   std::unique_ptr<Ui::WalletKeysCreateWidget> ui_;
   std::string walletId_;
   std::vector<std::unique_ptr<WalletKeyWidget>> widgets_;
   std::vector<bs::wallet::PasswordData> pwdData_;
   bs::wallet::KeyRank keyRank_ = { 0, 0 };
   Flags flags_{NoFlag};
   std::shared_ptr<ApplicationSettings> appSettings_;
   QString username_;
   MobileClient::RequestType requestType_{};
};

Q_DECLARE_OPERATORS_FOR_FLAGS(WalletKeysCreateWidget::Flags)

#endif // __WALLET_KEYS_CREATE_WIDGET_H__
