#ifndef __SELECT_WALLET_DIALOG_H__
#define __SELECT_WALLET_DIALOG_H__

#include <QDialog>
#include <memory>

namespace Ui {
    class SelectWalletDialog;
};
namespace bs {
   class Wallet;
}

class WalletsManager;
class WalletsViewModel;
class ApplicationSettings;

class SelectWalletDialog : public QDialog
{
Q_OBJECT

public:
   SelectWalletDialog(const std::shared_ptr<WalletsManager>& walletsManager, const std::string &selWalletId, QWidget* parent = nullptr);
   ~SelectWalletDialog() override = default;

   std::shared_ptr<bs::Wallet> getSelectedWallet() const;
   bool isNestedSegWitAddress() const;

public slots:
   void onSelectionChanged();
   void onDoubleClicked(const QModelIndex& index);

private:
   Ui::SelectWalletDialog*  ui_;
   WalletsViewModel  *              walletsModel_;
   std::shared_ptr<bs::Wallet>      selectedWallet_;
   std::shared_ptr<WalletsManager>  walletsManager_;
};

#endif // __SELECT_WALLET_DIALOG_H__
