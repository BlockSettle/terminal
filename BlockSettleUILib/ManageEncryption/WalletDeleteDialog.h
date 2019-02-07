#ifndef __WALLET_DELETE_DIALOG_H__
#define __WALLET_DELETE_DIALOG_H__

#include <QDialog>
#include <memory>
#include "BinaryData.h"


namespace spdlog
{
   class logger;
}
namespace Ui {
   class WalletDeleteDialog;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
   class Wallet;
}
class SignContainer;
class WalletsManager;
class ApplicationSettings;

class WalletDeleteDialog : public QDialog
{
   Q_OBJECT

public:
   WalletDeleteDialog(const std::shared_ptr<bs::hd::Wallet> &
      , const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<spdlog::logger> &logger
      , QWidget *parent = nullptr
      , bool fixedCheckBoxes = false, bool delRemote = false);
   WalletDeleteDialog(const std::shared_ptr<bs::Wallet> &
      , const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<spdlog::logger> &logger
      , QWidget *parent = nullptr);
   ~WalletDeleteDialog() override;

private slots:
   void doDelete();
   void onConfirmClicked();

private:
   void init();
   void initFixed();
   void deleteHDWallet();
   void deleteWallet();

   std::unique_ptr<Ui::WalletDeleteDialog> ui_;
   std::shared_ptr<bs::hd::Wallet>  hdWallet_;
   std::shared_ptr<bs::Wallet>      wallet_;
   std::shared_ptr<WalletsManager>  walletsManager_;
   std::shared_ptr<SignContainer>   signingContainer_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<spdlog::logger>     logger_;
   const bool fixedCheckBoxes_;
   const bool delRemoteWallet_;
};

#endif // __WALLET_DELETE_DIALOG_H__
