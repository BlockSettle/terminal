#ifndef __ADDRESS_DETAIL_DIALOG_H__
#define __ADDRESS_DETAIL_DIALOG_H__

#include <QDialog>
#include <memory>
#include "Address.h"


namespace Ui {
   class AddressDetailDialog;
}
namespace bs {
   class Wallet;
}

class WalletsManager;
class PyBlockDataManager;
class Tx;


class AddressDetailDialog : public QDialog
{
Q_OBJECT

public:
   AddressDetailDialog(const bs::Address &address, const std::shared_ptr<bs::Wallet> &wallet
      , const std::shared_ptr<WalletsManager>& walletsManager, QWidget* parent = nullptr );
   ~AddressDetailDialog() override = default;

private slots:
   void onCopyClicked() const;

private:
   void onError();

private:
   Ui::AddressDetailDialog *  ui_;
   const bs::Address          address_;
   std::shared_ptr<WalletsManager> walletsManager_;
   std::shared_ptr<bs::Wallet> wallet_;
};

#endif // __ADDRESS_DETAIL_DIALOG_H__
