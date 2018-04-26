#ifndef __NEW_WALLET_DIALOG_H__
#define __NEW_WALLET_DIALOG_H__

#include <QDialog>
#include <memory>
#include "BinaryData.h"


namespace Ui {
   class NewWalletDialog;
}


class NewWalletDialog : public QDialog
{
   Q_OBJECT

public:
   NewWalletDialog(bool noWalletsFound, QWidget *parent = nullptr);
   ~NewWalletDialog() noexcept override = default;

   bool isCreate() const { return isCreate_; }
   bool isImport() const { return isImport_; }

private:
   Ui::NewWalletDialog *ui_;
   bool  isCreate_ = false;
   bool  isImport_ = false;
};

#endif // __NEW_WALLET_DIALOG_H__
