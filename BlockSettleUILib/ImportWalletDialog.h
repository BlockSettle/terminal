/*

***********************************************************************************
* Copyright (C) 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __IMPORT_WALLET_DIALOG_H__
#define __IMPORT_WALLET_DIALOG_H__

#include <QDialog>
#include <memory>
#include "SeedDialog.h"
#include "SecureBinaryData.h"

namespace bs {
   namespace gui {
      namespace qt {
         class ImportWalletDialog : public QDialog
         {
            Q_OBJECT

         public:
            ImportWalletDialog(const std::string& rootId, QWidget* parent = nullptr);
            ~ImportWalletDialog() override;

            WalletSeedData getData() const { return data_; }

         private slots:
            void onClose();
            void onDataAvail();
            void onPasswordEdited();
            void on12WordsChanged();

         private:
            std::unique_ptr<Ui::SeedDialog>  ui_;
            std::string    walletId_;
            QPushButton* okButton_;
            WalletSeedData data_;
         };
      }
   }
}

#endif // __IMPORT_WALLET_DIALOG_H__
