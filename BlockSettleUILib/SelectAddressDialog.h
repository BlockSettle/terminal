#ifndef __SELECT_ADDRESS_DIALOG_H__
#define __SELECT_ADDRESS_DIALOG_H__

#include <QDialog>
#include <memory>

#include "Address.h"
#include "AddressListModel.h"

namespace Ui {
    class SelectAddressDialog;
}
namespace bs {
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}


class SelectAddressDialog : public QDialog
{
Q_OBJECT

public:
   SelectAddressDialog(const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<bs::sync::Wallet> &, QWidget* parent = nullptr
      , AddressListModel::AddressType addrType = AddressListModel::AddressType::All);
   ~SelectAddressDialog() override;

   bs::Address getSelectedAddress() const;

public slots:
   void onSelectionChanged();
   void onDoubleClicked(const QModelIndex& index);

private:
   bs::Address getAddress(const QModelIndex& index) const;

private:
   std::unique_ptr<Ui::SelectAddressDialog>  ui_;
   std::shared_ptr<bs::sync::Wallet>         wallet_;
   AddressListModel*             model_;
   bs::Address                   selectedAddr_;
};

#endif // __SELECT_ADDRESS_DIALOG_H__
