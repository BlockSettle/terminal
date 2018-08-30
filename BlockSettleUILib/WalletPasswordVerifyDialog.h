#ifndef __WALLETPASSWORDVERIFYDIALOG_H__
#define __WALLETPASSWORDVERIFYDIALOG_H__

#include <memory>
#include <QDialog>
#include "WalletEncryption.h"

namespace Ui {
   class WalletPasswordVerifyDialog;
}

class WalletPasswordVerifyDialog : public QDialog
{
   Q_OBJECT

public:
   enum Pages {
      FrejaInfo,
      Check,
   };

   explicit WalletPasswordVerifyDialog(QWidget *parent = nullptr);
   ~WalletPasswordVerifyDialog() override;

   // By default the dialog will show only Freja usage info page.
   // If init called then password/Freja check will be used too.
   void init(const std::string& walletId, const std::vector<bs::wallet::PasswordData>& keys
      , bs::wallet::KeyRank keyRank);

private slots:
   void onContinueClicked();

private:
   void initPassword();
   void initFreja(const QString& frejaId);

   std::unique_ptr<Ui::WalletPasswordVerifyDialog> ui_;
   std::string walletId_;
   std::vector<bs::wallet::PasswordData> keys_;
   bs::wallet::KeyRank keyRank_;
   bool runPasswordCheck_ = false;
};

#endif // __WALLETPASSWORDVERIFYDIALOG_H__
