#ifndef __VERIFY_WALLET_BACKUP_DIALOG_H__
#define __VERIFY_WALLET_BACKUP_DIALOG_H__

#include <QDialog>
#include <memory>
#include "BtcDefinitions.h"


namespace spdlog
{
   class logger;
}
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
   VerifyWalletBackupDialog(const std::shared_ptr<bs::hd::Wallet> &
                            , const std::shared_ptr<spdlog::logger> &logger
                            , QWidget *parent = nullptr);
   ~VerifyWalletBackupDialog() override;

private slots:
   void onPrivKeyChanged();

private:
   std::unique_ptr<Ui::VerifyWalletBackupDialog> ui_;
   std::shared_ptr<bs::hd::Wallet>     wallet_;
   const NetworkType    netType_;
   std::shared_ptr<EasyCoDec> easyCodec_;
   std::unique_ptr<EasyEncValidator> validator_;
   std::shared_ptr<spdlog::logger>     logger_;
};

#endif // __VERIFY_WALLET_BACKUP_DIALOG_H__
