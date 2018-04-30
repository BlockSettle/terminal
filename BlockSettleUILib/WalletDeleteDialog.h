#ifndef __WALLET_DELETE_DIALOG_H__
#define __WALLET_DELETE_DIALOG_H__

#include <QDialog>
#include <memory>
#include "BinaryData.h"


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


class WalletDeleteDialog : public QDialog
{
   Q_OBJECT

public:
   WalletDeleteDialog(const std::shared_ptr<bs::hd::Wallet> &, const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<SignContainer> &, QWidget *parent = nullptr);
   WalletDeleteDialog(const std::shared_ptr<bs::Wallet> &, const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<SignContainer> &, QWidget *parent = nullptr);
   ~WalletDeleteDialog() noexcept override = default;

private slots:
   void doDelete();
   void onConfirmClicked();

private:
   void init();
   void deleteHDWallet();
   void deleteWallet();

private:
   Ui::WalletDeleteDialog *ui_;
   std::shared_ptr<bs::hd::Wallet>  hdWallet_;
   std::shared_ptr<bs::Wallet>      wallet_;
   std::shared_ptr<WalletsManager>  walletsManager_;
   std::shared_ptr<SignContainer>   signingContainer_;
};

#endif // __WALLET_DELETE_DIALOG_H__
