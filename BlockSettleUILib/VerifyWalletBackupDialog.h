#ifndef __VERIFY_WALLET_BACKUP_DIALOG_H__
#define __VERIFY_WALLET_BACKUP_DIALOG_H__

#include <QDialog>
#include <memory>
#include "BtcDefinitions.h"


namespace Ui {
   class VerifyWalletBackupDialog;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
}
class EasyCoDec;
class EasyEncValidator;


class VerifyWalletBackupDialog : public QDialog
{
   Q_OBJECT

public:
   VerifyWalletBackupDialog(const std::shared_ptr<bs::hd::Wallet> &, QWidget *parent = nullptr);
   ~VerifyWalletBackupDialog() noexcept override;

private slots:
   void onPrivKeyChanged();

private:
   Ui::VerifyWalletBackupDialog *ui_;
   std::shared_ptr<bs::hd::Wallet>     wallet_;
   const NetworkType    netType_;
   std::shared_ptr<EasyCoDec> easyCodec_;
   EasyEncValidator         * validator_ = nullptr;
};

#endif // __VERIFY_WALLET_BACKUP_DIALOG_H__
