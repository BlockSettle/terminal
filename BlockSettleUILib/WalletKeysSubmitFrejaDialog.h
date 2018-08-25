#ifndef __WALLETKEYSSUBMITFREJADIALOG_H__
#define __WALLETKEYSSUBMITFREJADIALOG_H__

#include <QDialog>

#include "EncryptionUtils.h"
#include "FrejaREST.h"
#include "WalletEncryption.h"
#include <QElapsedTimer>

class QPropertyAnimation;

namespace Ui {
   class WalletKeysSubmitFrejaDialog;
}

class WalletKeysSubmitFrejaDialog : public QDialog
{
   Q_OBJECT

public:
   WalletKeysSubmitFrejaDialog(const std::string &walletId
      , bs::wallet::KeyRank keyRank, const std::vector<bs::wallet::EncryptionType> &encTypes
      , const std::vector<SecureBinaryData> &encKeys, const QString &prompt, QWidget* parent = nullptr);
   ~WalletKeysSubmitFrejaDialog() override;

   SecureBinaryData GetPassword() const;

private slots:
   void cancel();
   void onSucceeded();
   void onFailed();

private:
   QPropertyAnimation* startAnimation(bool success);

   std::unique_ptr<Ui::WalletKeysSubmitFrejaDialog> ui_;
};

#endif // __WALLETKEYSSUBMITFREJADIALOG_H__
