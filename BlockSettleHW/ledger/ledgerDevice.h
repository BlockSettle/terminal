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
#include "hwdeviceinterface.h"
#include "ledger/hidapi/hidapi.h"

#include <QThread>

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

class LedgerCommandThread;
class LedgerDevice : public HwDeviceInterface
{
   Q_OBJECT

public:
   LedgerDevice(HidDeviceInfo&& hidDeviceInfo, bool testNet,
      std::shared_ptr<bs::sync::WalletsManager> walletManager, const std::shared_ptr<spdlog::logger> &logger, QObject* parent = nullptr);
   ~LedgerDevice() override;

   DeviceKey key() const override;
   DeviceType type() const override;

   // lifecycle
   void init(AsyncCallBack&& cb = nullptr) override;
   void cancel() override;

   // operation
   void getPublicKey(AsyncCallBackCall&& cb = nullptr) override;
   void signTX(const QVariant& reqTX, AsyncCallBackCall&& cb = nullptr) override;

private:
   LedgerCommandThread *blankCommand(AsyncCallBackCall&& cb = nullptr);

private:
   HidDeviceInfo hidDeviceInfo_;
   bool testNet_{};
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<bs::sync::WalletsManager> walletManager_;

   
};

class LedgerCommandThread : public QThread
{
   Q_OBJECT
public:
   LedgerCommandThread(const HidDeviceInfo &hidDeviceInfo, bool testNet,
      const std::shared_ptr<spdlog::logger> &logger, QObject *parent = nullptr);
   ~LedgerCommandThread() override;

   void run() override;

   void prepareGetPublicKey(const DeviceKey &deviceKey);
   void prepareSignTx(const DeviceKey &deviceKey, bs::core::wallet::TXSignRequest&& coreReq
      , std::vector<bs::hd::Path>&& paths, bs::hd::Path&& changePath);

signals:
   void resultReady(QVariant const &result);
   void error();

protected:
   // Device management
   bool initDevice();
   void releaseDevice();

   // APDU commands processing
   bool exchangeData(const QByteArray& input, QByteArray& output, std::string&& logHeader);
   bool writeData(const QByteArray& input, std::string&& logHeader);
   bool readData(QByteArray& output, std::string&& logHeader);

   // Get public key processing
   void processGetPublicKey();
   BIP32_Node retrievePublicKeyFromPath(bs::hd::Path&& derivationPath);
   BIP32_Node getPublicKeyApdu(bs::hd::Path&& derivationPath, const std::unique_ptr<BIP32_Node>& parent = nullptr);

   // Sign tx processing
   void processTXSigning();

private:
   HidDeviceInfo hidDeviceInfo_;
   bool testNet_{};
   std::shared_ptr<spdlog::logger> logger_;
   hid_device* dongle_ = nullptr;

   enum class HardwareCommand {
      None,
      GetPublicKey,
      SignTX
   };

   // Thread purpose data
   HardwareCommand threadPurpose_;
   DeviceKey deviceKey_;
   std::unique_ptr<bs::core::wallet::TXSignRequest> coreReq_{};
   std::vector<bs::hd::Path> inputPaths_;
   bs::hd::Path changePath_;
};

#endif // LEDGERDEVICE_H
