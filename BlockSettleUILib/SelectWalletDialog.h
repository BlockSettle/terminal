#ifndef __SELECT_WALLET_DIALOG_H__
#define __SELECT_WALLET_DIALOG_H__

#include <QDialog>
#include <memory>

namespace Ui {
    class SelectWalletDialog;
}
namespace bs {
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}
class WalletsViewModel;
class ApplicationSettings;


class SelectWalletDialog : public QDialog
{
Q_OBJECT

public:
   SelectWalletDialog(const std::shared_ptr<bs::sync::WalletsManager> &, const std::string &selWalletId, QWidget* parent = nullptr);
   ~SelectWalletDialog() override;

   std::shared_ptr<bs::sync::Wallet> getSelectedWallet() const;

public slots:
   void onSelectionChanged();
   void onDoubleClicked(const QModelIndex& index);

private:
   std::unique_ptr<Ui::SelectWalletDialog> ui_;
   WalletsViewModel  *              walletsModel_;
   std::shared_ptr<bs::sync::Wallet>         selectedWallet_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
};

#endif // __SELECT_WALLET_DIALOG_H__
