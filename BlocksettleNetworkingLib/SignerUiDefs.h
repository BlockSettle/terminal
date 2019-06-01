#ifndef __SIGNER_UI_DEFS_H__
#define __SIGNER_UI_DEFS_H__

#include "QObject"
#include "QString"
#include "WalletEncryption.h"
#include "SignerDefs.h"

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

         default:
            return QStringLiteral("");
         }
      }

      // List of signer run modes
      enum class RunMode {
         fullgui,
         lightgui,
         headless,
         cli
      };
      Q_ENUM_NS(RunMode)

      inline QString TxErrorCodeString(bs::sync::TxErrorCode errorCode) {
         switch (errorCode) {
         case bs::sync::TxErrorCode::NoError:
            return QObject::tr("");
         case bs::sync::TxErrorCode::Canceled:
            return QObject::tr("Transaction canceled");
         case bs::sync::TxErrorCode::SpendLimitExceed:
            return QObject::tr("Spend limit exceeded");
         case bs::sync::TxErrorCode::FailedToParse:
            return QObject::tr("Failed to parse Sign TX Request");
         case bs::sync::TxErrorCode::InvalidTxRequest:
            return QObject::tr("Tx Request is invalid, missing critical data");
         case bs::sync::TxErrorCode::WalletNotFound:
            return QObject::tr("Failed to find wallet");
         case bs::sync::TxErrorCode::MissingPassword:
            return QObject::tr("Missing password for encrypted wallet");
         case bs::sync::TxErrorCode::MissingAuthKeys:
            return QObject::tr("Missing auth priv/pub keys for encrypted wallet");
         case bs::sync::TxErrorCode::MissingSettlementWallet:
            return QObject::tr("Missing settlement wallet");
         case bs::sync::TxErrorCode::MissingAuthWallet:
            return QObject::tr("Missing Auth wallet");
         default:
            return QObject::tr("Unknown error");
         }
      }

      }  // namespace ui
   }  // namespace signer
}  // namespace bs

#endif
