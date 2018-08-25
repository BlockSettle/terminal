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
  WalletKeysSubmitFrejaDialog(const QString& walletName, const std::string &walletId
    , const std::vector<SecureBinaryData> &encKeys, const QString &prompt, QWidget* parent = nullptr);
  ~WalletKeysSubmitFrejaDialog() override;

  SecureBinaryData GetPassword() const;

private slots:
  void cancel();

private:
  void onFrejaSucceeded(SecureBinaryData password);
  void onFrejaFailed(const QString &text);
  void onTimer();
  QPropertyAnimation* startAnimation(bool success);

  Ui::WalletKeysSubmitFrejaDialog *ui_;

  QElapsedTimer timeout_;
  FrejaSignWallet frejaSign_;
  QTimer timer_;
  SecureBinaryData password_;
};

#endif // __WALLETKEYSSUBMITFREJADIALOG_H__
