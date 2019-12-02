/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __NEW_WALLET_DIALOG_H__
#define __NEW_WALLET_DIALOG_H__

#include <QDialog>

#include <memory>

namespace Ui {
   class NewWalletDialog;
}

class ApplicationSettings;

class NewWalletDialog : public QDialog
{
   Q_OBJECT

public:
   NewWalletDialog(bool noWalletsFound, const std::shared_ptr<ApplicationSettings>& appSettings, QWidget *parent = nullptr);
   ~NewWalletDialog() override;

   bool isCreate() const { return isCreate_; }
   bool isImport() const { return isImport_; }

private:
   std::unique_ptr<Ui::NewWalletDialog> ui_;
   bool  isCreate_ = false;
   bool  isImport_ = false;
};

#endif // __NEW_WALLET_DIALOG_H__
