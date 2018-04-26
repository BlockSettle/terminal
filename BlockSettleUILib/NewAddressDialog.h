#ifndef __NEW_ADDRESS_DIALOG_H__
#define __NEW_ADDRESS_DIALOG_H__

#include <QDialog>
#include "MetaData.h"
#include <memory>

namespace Ui {
   class NewAddressDialog;
}
class SignContainer;


class NewAddressDialog : public QDialog
{
Q_OBJECT

public:
   NewAddressDialog(const std::shared_ptr<bs::Wallet>& wallet, const std::shared_ptr<SignContainer> &
      , bool isNested = false, QWidget* parent = nullptr);
   ~NewAddressDialog() override = default;

private slots:
   void copyToClipboard();

private:
   Ui::NewAddressDialog* ui_;
   std::shared_ptr<bs::Wallet> wallet_;
   bs::Address    address_;
};

#endif // __NEW_ADDRESS_DIALOG_H__
