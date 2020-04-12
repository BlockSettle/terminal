/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef LEDGERDEVICE_H
#define LEDGERDEVICE_H

#include "ledger/ledgerStructure.h"
#include "hwdeviceabstract.h"
#include "ledger/hidapi/hidapi.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
   namespace core {
      namespace wallet {
         struct TXSignRequest;
      }
   }
}

class LedgerDevice : public HwDeviceAbstract
{
   Q_OBJECT

public:
   LedgerDevice(HidDeviceInfo&& hidDeviceInfo, bool testNet,
      std::shared_ptr<bs::sync::WalletsManager> walletManager, std::shared_ptr<spdlog::logger> logger, QObject* parent = nullptr);
   ~LedgerDevice() override;

   DeviceKey key() const override;
   DeviceType type() const override;

   // lifecycle
   void init(AsyncCallBack&& cb = nullptr) override;
   void cancel() override;

   // operation
   void getPublicKey(AsyncCallBackCall&& cb = nullptr) override;
   void signTX(const QVariant& reqTX, AsyncCallBackCall&& cb = nullptr) override;

protected:
   // Device management
   bool initDevice();
   void releaseDevice();

   // APDU commands processing
   bool exchangeData(const QByteArray& input, QByteArray& output, std::string&& logHeader);
   bool writeData(const QByteArray& input, std::string&& logHeader);
   bool readData(QByteArray& output, std::string&& logHeader);

   // Get public key processing
   void processGetPublicKey(AsyncCallBackCall&& cb = nullptr);
   BIP32_Node retrievePublicKeyFromPath(std::vector<uint32_t>&& derivationPath);
   BIP32_Node getPublicKeyApdu(std::vector<uint32_t>&& derivationPath, const std::unique_ptr<BIP32_Node>& parent = nullptr);

   // Sign transaction processing
   //std::vector<BIP32_Node> pubKeyPerUtxo(bs::core::wallet::TXSignRequest& coreReq);

private:
   HidDeviceInfo hidDeviceInfo_;
   bool testNet_{};
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<bs::sync::WalletsManager> walletManager_;

   hid_device* dongle_ = nullptr;
};

#endif // LEDGERDEVICE_H
