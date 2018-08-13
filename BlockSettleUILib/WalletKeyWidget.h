#ifndef __WALLET_KEY_WIDGET_H__
#define __WALLET_KEY_WIDGET_H__

#include <QTimer>
#include <QWidget>
#include "EncryptionUtils.h"
#include "FrejaREST.h"

namespace Ui {
    class WalletKeyWidget;
}


class WalletKeyWidget : public QWidget
{
   Q_OBJECT
public:
   WalletKeyWidget(const std::string &walletId, int index, bool password, QWidget* parent = nullptr);
   ~WalletKeyWidget() override = default;

   void cancel();
   void start();

   void setEncryptionKeys(const std::vector<SecureBinaryData> &encKeys, int index = 0);
   void setFixedType(bool on = true);
   void setFocus();

signals:
   void keyChanged(int index, SecureBinaryData);
   void encKeyChanged(int index, SecureBinaryData);
   void keyTypeChanged(int index, bool password);

private slots:
   void onTypeChanged();
   void onPasswordChanged();
   void onFrejaIdChanged(const QString &);
   void onFrejaSignClicked();
   void onFrejaSucceeded(SecureBinaryData);
   void onFrejaFailed(const QString &text);
   void onFrejaStatusUpdated(const QString &status);
   void onTimer();

private:
   void stop();

private:
   Ui::WalletKeyWidget* ui_;
   std::string walletId_;
   int         index_;
   bool        password_;
   bool        frejaRunning_ = false;
   FrejaSignWallet   frejaSign_;
   QTimer      timer_;
   float       timeLeft_;
};

#endif // __WALLET_KEY_WIDGET_H__
