#ifndef __ENTER_WALLET_PASSWORD_H__
#define __ENTER_WALLET_PASSWORD_H__

#include <QDialog>
#include <QTimer>


namespace Ui {
    class EnterWalletPassword;
};

class EnterWalletPassword : public QDialog
{
Q_OBJECT

public:
   EnterWalletPassword(const QString& walletName, const QString &prompt = {}, QWidget* parent = nullptr);
   ~EnterWalletPassword() override = default;

   QString GetPassword() const;

private slots:
   void PasswordChanged();

private:
   Ui::EnterWalletPassword* ui_;
   QTimer   timer_;
   float    timeLeft_ = 120;
};

#endif // __ENTER_WALLET_PASSWORD_H__
