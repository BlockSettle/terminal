#ifndef __SIGNER_UI_DEFS_H__
#define __SIGNER_UI_DEFS_H__

#include "QObject"
#include "QString"
#include "WalletEncryption.h"
#include "BSErrorCode.h"

namespace bs {
   namespace signer {
      namespace ui {

      Q_NAMESPACE

      // List of callable Signer dialogs
      enum class DialogType {
         CreateWallet,
         ImportWallet,
         BackupWallet,
         DeleteWallet,
         ManageWallet,
         ActivateAutoSign,
      };
      Q_ENUM_NS(DialogType)

      inline QString getSignerDialogPath(DialogType signerDialog) {
         switch (signerDialog) {
         case DialogType::CreateWallet:
            return QStringLiteral("createNewWalletDialog");
         case DialogType::ImportWallet:
            return QStringLiteral("importWalletDialog");
         case DialogType::BackupWallet:
            return QStringLiteral("backupWalletDialog");
         case DialogType::DeleteWallet:
            return QStringLiteral("deleteWalletDialog");
         case DialogType::ManageWallet:
            return QStringLiteral("manageEncryptionDialog");
         case DialogType::ActivateAutoSign:
            return QStringLiteral("activateAutoSignDialog");
         }

         assert(false);
         return QStringLiteral("");
      }

      // List of signer run modes
      enum class RunMode {
         fullgui,
         litegui,
         headless,
         cli
      };
      Q_ENUM_NS(RunMode)

      }  // namespace ui
   }  // namespace signer
}  // namespace bs

#endif
