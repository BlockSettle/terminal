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
   enum Flag {
      NoFlag = 0x00,
      HideFrejaConnectButton = 0x01,
      HideFrejaCombobox = 0x02,
      HideGroupboxCaption = 0x04,
      FrejaProgressBarFixed = 0x08,
      FrejaIdVisible = 0x10,
   };
   Q_DECLARE_FLAGS(Flags, Flag)

   WalletKeysSubmitWidget(QWidget* parent = nullptr);
   ~WalletKeysSubmitWidget() override;

   void setFlags(Flags flags);
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
   void failed();

private slots:
   void onKeyChanged(int index, SecureBinaryData);
   void onKeyTypeChanged(int index, bool password);
   void onEncKeyChanged(int index, SecureBinaryData);

private:
   void addKey(bool password, const std::vector<SecureBinaryData> &encKeys
      , int encKeyIndex = 0, bool isFixed = false);

private:
   std::unique_ptr<Ui::WalletKeysSubmitWidget> ui_;
   std::string    walletId_;
   std::vector<WalletKeyWidget *>         widgets_;
   std::vector<bs::wallet::PasswordData>  pwdData_;
   std::atomic_bool  suspended_;
   Flags flags_{NoFlag};
};

Q_DECLARE_OPERATORS_FOR_FLAGS(WalletKeysSubmitWidget::Flags)

#endif // __WALLET_KEYS_SUBMIT_WIDGET_H__
