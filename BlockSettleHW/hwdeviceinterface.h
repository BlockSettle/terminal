/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef HWDEVICEABSTRACT_H
#define HWDEVICEABSTRACT_H

#include <string>
#include "CoreWallet.h"
#include "SecureBinaryData.h"

namespace bs {
   namespace core {
      namespace wallet {
         struct TXSignRequest;
      }
   }
}

namespace bs {
   namespace hww {
      enum class DeviceType {
         Unknown,
         HWLedger,
         HWTrezor
      };

      struct DeviceKey
      {
         std::string label;
         std::string id;
         std::string vendor;
         std::string walletId;
         std::string status;
         DeviceType type{ DeviceType::Unknown };
      };

      class DeviceInterface
      {
      public:
         virtual DeviceKey key() const = 0;
         virtual DeviceType type() const = 0;

         // lifecycle
         virtual void init() = 0;
         virtual void cancel() = 0;
         virtual void clearSession() = 0;

         // operation
         virtual void getPublicKeys() = 0;
         virtual void signTX(const bs::core::wallet::TXSignRequest& reqTX) = 0;
         virtual void retrieveXPubRoot() = 0;

         // Management
         virtual void setMatrixPin(const SecureBinaryData& pin) {};
         virtual void setPassword(const SecureBinaryData& password, bool enterOnDevice) {};

         // State
         virtual bool isBlocked() const = 0;
         virtual std::string lastError() const { return {}; };

         virtual bool inited()
         {
            return !xpubRoot_.empty();
         }

         // operation result informing
         virtual void publicKeyReady() = 0;
         virtual void deviceTxStatusChanged(const std::string& status) = 0;
         virtual void operationFailed(const std::string& reason) = 0;
         virtual void requestForRescan() = 0;

         // Management
         virtual void requestPinMatrix() = 0;
         virtual void requestHWPass(bool allowedOnDevice) = 0;
         virtual void cancelledOnDevice() = 0;
         virtual void invalidPin() = 0;

      protected:
         BinaryData xpubRoot_;
      };

   }  //hw
}  //bs

#endif // HWDEVICEABSTRACT_H
