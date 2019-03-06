#ifndef __WALLET_KEYS_CREATE_WIDGET_H__
#define __WALLET_KEYS_CREATE_WIDGET_H__

#include <QWidget>
#include "WalletEncryption.h"
#include "AutheIDClient.h"
#include "QWalletInfo.h"
#include "WalletKeyWidget.h"

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
   void init(AutheIDClient::RequestType requestType
             , const bs::hd::WalletInfo &walletInfo
             , WalletKeyWidget::UseType useType
             , const std::shared_ptr<ApplicationSettings>& appSettings
             , const std::shared_ptr<ConnectionManager> &connectionManager
             , const std::shared_ptr<spdlog::logger> &logger);

   void cancel();

   bool isValid() const;
   bs::wallet::PasswordData passwordData(int keyIndex) const { return pwdData_.at(keyIndex); }
   std::vector<bs::wallet::PasswordData> passwordData() const { return pwdData_; }
   bs::wallet::KeyRank keyRank() const { return keyRank_; }

public slots:
   void setFocus();

signals:
   void keyChanged();
   void keyCountChanged();
   void failed();
   //void keyTypeChanged(bool password);
   void returnPressed();

private slots:
   void onAddClicked();
   void onDelClicked();
   void onPasswordDataChanged(int index, bs::wallet::PasswordData passwordData);
   void updateKeyRank(int);

private:
   void addKey();

private:
   std::unique_ptr<Ui::WalletKeysCreateWidget> ui_;
   std::vector<std::unique_ptr<WalletKeyWidget>> widgets_;
   std::vector<bs::wallet::PasswordData> pwdData_;
   bs::wallet::KeyRank keyRank_ = { 0, 0 };
   Flags flags_{NoFlag};
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager> connectionManager_;
   QString username_;
   AutheIDClient::RequestType requestType_{};
   bs::hd::WalletInfo walletInfo_;
   std::shared_ptr<spdlog::logger> logger_;
   WalletKeyWidget::UseType useType_;

};

Q_DECLARE_OPERATORS_FOR_FLAGS(WalletKeysCreateWidget::Flags)

#endif // __WALLET_KEYS_CREATE_WIDGET_H__
