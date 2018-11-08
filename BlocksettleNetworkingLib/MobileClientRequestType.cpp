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
   case MobileClientRequest::DeactivateWalletDevice:
      return app->tr("Deactivate wallet device");
   case MobileClientRequest::VerifyWalletKey:
      return app->tr("Verify wallet key");
   case MobileClientRequest::ActivateOTP:
      return app->tr("Activate OTP");
   default:
      throw std::logic_error("Invalid MobileClientRequest value");
   }
}

bool isMobileClientNewDeviceNeeded(MobileClientRequest requestType)
{
   switch (requestType) {
   case MobileClientRequest::ActivateWallet:
   case MobileClientRequest::ActivateWalletNewDevice:
   case MobileClientRequest::ActivateOTP:
      return true;
   }

   return false;
}
