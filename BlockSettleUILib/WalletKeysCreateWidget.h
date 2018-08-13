#ifndef __WALLET_KEYS_CREATE_WIDGET_H__
#define __WALLET_KEYS_CREATE_WIDGET_H__

#include <QWidget>
#include "WalletEncryption.h"

namespace Ui {
    class WalletKeysCreateWidget;
}
class WalletKeyWidget;


class WalletKeysCreateWidget : public QWidget
{
   Q_OBJECT
public:
   WalletKeysCreateWidget(QWidget* parent = nullptr);
   ~WalletKeysCreateWidget() override = default;

   void init(const std::string &walletId);
   void addPasswordKey() { addKey(true); }
   void addFrejaKey() { addKey(false); }
   void cancel();

   bool isValid() const;
   std::vector<bs::wallet::PasswordData> keys() const { return pwdData_; }
   bs::wallet::KeyRank keyRank() const { return keyRank_; }

signals:
   void keyChanged();
   void keyCountChanged();

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
   Ui::WalletKeysCreateWidget *  ui_;
   std::string    walletId_;
   std::vector<WalletKeyWidget *>      widgets_;
   std::vector<bs::wallet::PasswordData>  pwdData_;
   bs::wallet::KeyRank                 keyRank_ = { 0, 0 };
};

#endif // __WALLET_KEYS_CREATE_WIDGET_H__
