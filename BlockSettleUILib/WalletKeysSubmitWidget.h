#ifndef __WALLET_KEYS_SUBMIT_WIDGET_H__
#define __WALLET_KEYS_SUBMIT_WIDGET_H__

#include <QWidget>
#include "WalletEncryption.h"

namespace Ui {
    class WalletKeysSubmitWidget;
}
class WalletKeyWidget;


class WalletKeysSubmitWidget : public QWidget
{
   Q_OBJECT
public:
   WalletKeysSubmitWidget(QWidget* parent = nullptr);
   ~WalletKeysSubmitWidget() override = default;

   void init(const std::string &walletId, bs::wallet::KeyRank
      , const std::vector<bs::wallet::EncryptionType> &
      , const std::vector<SecureBinaryData> &encKeys);
   void cancel();

   bool isValid() const;
   SecureBinaryData key() const;

   void setFocus();
   void suspend() { suspended_ = true; }
   void resume();

signals:
   void keyChanged();
   void keyCountChanged();

private slots:
   void onKeyChanged(int index, SecureBinaryData);
   void onKeyTypeChanged(int index, bool password);
   void onEncKeyChanged(int index, SecureBinaryData);

private:
   void addKey(bool password, const std::vector<SecureBinaryData> &encKeys
      , int encKeyIndex = 0, bool isFixed = false);

private:
   Ui::WalletKeysSubmitWidget *  ui_;
   std::string    walletId_;
   std::vector<WalletKeyWidget *>         widgets_;
   std::vector<bs::wallet::PasswordData>  pwdData_;
   std::atomic_bool  suspended_;
};

#endif // __WALLET_KEYS_SUBMIT_WIDGET_H__
