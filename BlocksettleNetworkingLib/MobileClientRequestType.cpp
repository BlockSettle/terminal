#include "MobileClientRequestType.h"

#include <QCoreApplication>
#include <exception>

QString getMobileClientRequestText(MobileClientRequest requestType)
{
   auto app = QCoreApplication::instance();

   switch (requestType) {
   case MobileClientRequest::ActivateWallet:
      return app->tr("Activate wallet");
   case MobileClientRequest::DeactivateWallet:
      return app->tr("Deactivate wallet");
   case MobileClientRequest::SignWallet:
      return app->tr("Sign transaction");
   case MobileClientRequest::BackupWallet:
      return app->tr("Backup wallet");
   case MobileClientRequest::ActivateWalletOldDevice:
      return app->tr("Activate wallet (existing device)");
   case MobileClientRequest::ActivateWalletNewDevice:
      return app->tr("Activate wallet (new device)");
   case MobileClientRequest::VerifyWalletKey:
      return app->tr("Verify wallet key");
   default:
      throw std::logic_error("Invalid MobileClientRequest value");
   }
}

bool isMobileClientNewDeviceNeeded(MobileClientRequest requestType)
{
   switch (requestType) {
   case MobileClientRequest::ActivateWallet:
   case MobileClientRequest::ActivateWalletNewDevice:
      return true;
   }

   return false;
}
