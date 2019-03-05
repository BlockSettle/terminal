#ifndef __WALLET_DELETE_DIALOG_H__
#define __WALLET_DELETE_DIALOG_H__

#include <QDialog>
#include <memory>
#include "BinaryData.h"

namespace spdlog {
   class logger;
}
namespace Ui {
   class WalletDeleteDialog;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }
}
class SignContainer;
class ApplicationSettings;
class ConnectionManager;


class WalletDeleteDialog : public QDialog
{
   Q_OBJECT

public:
   WalletDeleteDialog(const std::shared_ptr<bs::sync::hd::Wallet> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , const std::shared_ptr<spdlog::logger> &logger
      , QWidget *parent = nullptr
      , bool fixedCheckBoxes = false, bool delRemote = false);
   WalletDeleteDialog(const std::shared_ptr<bs::sync::Wallet> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &connectionManager
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

   std::unique_ptr<Ui::WalletDeleteDialog>   ui_;
   std::shared_ptr<bs::sync::hd::Wallet>     hdWallet_;
   std::shared_ptr<bs::sync::Wallet>         wallet_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<SignContainer>   signingContainer_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager> connectionManager_;
   std::shared_ptr<spdlog::logger>     logger_;
   const bool fixedCheckBoxes_;
   const bool delRemoteWallet_;
};

#endif // __WALLET_DELETE_DIALOG_H__
