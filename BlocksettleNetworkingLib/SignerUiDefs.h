/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SIGNER_UI_DEFS_H__
#define __SIGNER_UI_DEFS_H__

#include "QObject"
#include "QString"
#include "WalletEncryption.h"
#include "BSErrorCode.h"
#include "QMetaEnum"

namespace bs {
   namespace signer {
      namespace ui {

      Q_NAMESPACE

      // List of callable Signer dialogs
      enum class GeneralDialogType {
         CreateWallet,
         ImportWallet,
         BackupWallet,
         DeleteWallet,
         ManageWallet,
         ActivateAutoSign,
      };
      Q_ENUM_NS(GeneralDialogType)

      inline QString getGeneralDialogName(GeneralDialogType signerDialog) {
         switch (signerDialog) {
         case GeneralDialogType::CreateWallet:
            return QStringLiteral("createNewWalletDialog");
         case GeneralDialogType::ImportWallet:
            return QStringLiteral("importWalletDialog");
         case GeneralDialogType::BackupWallet:
            return QStringLiteral("backupWalletDialog");
         case GeneralDialogType::DeleteWallet:
            return QStringLiteral("deleteWalletDialog");
         case GeneralDialogType::ManageWallet:
            return QStringLiteral("manageEncryptionDialog");
         case GeneralDialogType::ActivateAutoSign:
            return QStringLiteral("activateAutoSignDialog");
         }

         assert(false);
         return QStringLiteral("");
      }

      // List of callable Signer dialogs
      enum class PasswordInputDialogType {
         RequestPasswordForAuthLeaf,
         RequestPasswordForSettlementLeaf,
         RequestPasswordForToken,
         RequestPasswordForRevokeAuthAddress,
         RequestPasswordForPromoteHDWallet
      };
      Q_ENUM_NS(PasswordInputDialogType)

      inline QString getPasswordInputDialogName(PasswordInputDialogType signerDialog) {
         QMetaEnum metaEnum = QMetaEnum::fromType<PasswordInputDialogType>();
         return QString::fromLatin1(metaEnum.valueToKey(static_cast<int>(signerDialog)));
      }

      // List of signer run modes
      enum class RunMode {
         fullgui,
         litegui,
         headless,
         cli
      };
      Q_ENUM_NS(RunMode)

      // function name in helper.js which can call various password input dialogs
      static const char *createPasswordDialogForType = "createPasswordDialogForType";

      // function name in helper.js which can call general dialogs
      static const char *customDialogRequest = "customDialogRequest";

      // these strings are function names in helper.js which can be evaluated by name
      const QList<std::string> qmlCallableDialogMethods =
      {
         "createTxSignDialog",
         "createTxSignSettlementDialog",
         "updateDialogData",
         createPasswordDialogForType
      };

      }  // namespace ui
   }  // namespace signer
}  // namespace bs

#endif
