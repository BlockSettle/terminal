/*

***********************************************************************************
* Copyright (C) 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SEED_DIALOG_H__
#define __SEED_DIALOG_H__

#include <QDialog>
#include <memory>
#include "SecureBinaryData.h"

namespace Ui {
   class SeedDialog;
}
namespace bs {
   namespace sync {
      class Wallet;
   }
}
class QPushButton;

namespace bs {
   namespace gui {
      struct WalletSeedData
      {
         std::string name;
         std::string description;
         SecureBinaryData  password;
         SecureBinaryData  seed;
         std::string xpriv;

         bool empty() const
         {
            return (name.empty() || (seed.empty() && xpriv.empty()));
         }
      };

      namespace qt {
         class SeedDialog : public QDialog
         {
            Q_OBJECT

         public:
            SeedDialog(const std::string& rootId, QWidget* parent = nullptr);
            ~SeedDialog() override;

            WalletSeedData getData() const { return data_; }

         protected:
            void showEvent(QShowEvent* event) override;

         private slots:
            void onClose();
            void generateSeed();
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

#endif // __SEED_DIALOG_H__
