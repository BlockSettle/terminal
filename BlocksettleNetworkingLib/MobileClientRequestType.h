#ifndef __MOBILE_CLIENT_REQUEST_TYPE_H__
#define __MOBILE_CLIENT_REQUEST_TYPE_H__

#include <QString>

enum class MobileClientRequest
{
   Unknown,
   ActivateWallet,
   DeactivateWallet,
   SignWallet,
   BackupWallet,
   ActivateWalletOldDevice,
   ActivateWalletNewDevice,
   DeactivateWalletDevice,
   VerifyWalletKey,
   ActivateOTP,
   // Private market and others with lower timeout
   SettlementTransaction,

   // Please also add new type text in getMobileClientRequestText
};

QString getMobileClientRequestText(MobileClientRequest requestType);

bool isMobileClientNewDeviceNeeded(MobileClientRequest requestType);

int getMobileClientTimeout(MobileClientRequest requestType);

#endif // __MOBILE_CLIENT_REQUEST_TYPE_H__
