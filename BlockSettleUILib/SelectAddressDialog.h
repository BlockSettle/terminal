#ifndef __SELECT_ADDRESS_DIALOG_H__
#define __SELECT_ADDRESS_DIALOG_H__

#include <QDialog>
#include <memory>

#include "Address.h"
#include "AddressListModel.h"

namespace Ui {
    class SelectAddressDialog;
}

class WalletsManager;

namespace bs {
   class Wallet;
}

class SelectAddressDialog : public QDialog
{
Q_OBJECT

public:
   SelectAddressDialog(const std::shared_ptr<WalletsManager>& walletsManager
      , const std::shared_ptr<bs::Wallet>& wallet, QWidget* parent = nullptr
      , AddressListModel::AddressType addrType = AddressListModel::AddressType::All);
   ~SelectAddressDialog() override = default;

   bs::Address getSelectedAddress() const;

public slots:
   void onSelectionChanged();
   void onDoubleClicked(const QModelIndex& index);

private:
   bs::Address getAddress(const QModelIndex& index) const;

private:
   Ui::SelectAddressDialog*      ui_;
   std::shared_ptr<bs::Wallet>   wallet_;
   AddressListModel*             model_;
   bs::Address                   selectedAddr_;
};

#endif // __SELECT_ADDRESS_DIALOG_H__
