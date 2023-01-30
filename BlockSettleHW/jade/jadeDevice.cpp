/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QDataStream>
#include <spdlog/spdlog.h>
#include "hwdevicemanager.h"
#include "jadeDevice.h"
#include "jadeClient.h"
#include "CoreWallet.h"


using namespace bs::hww;

JadeDevice::JadeDevice(const std::shared_ptr<spdlog::logger> &logger
   , bool testNet, DeviceCallbacks* cb, const std::string& endpoint)
   : bs::WorkerPool(1, 1)
   , logger_(logger), testNet_(testNet), cb_(cb), endpoint_(endpoint)
{}

JadeDevice::~JadeDevice() = default;

#if 0
std::shared_ptr<bs::Worker> JadeDevice::worker(const std::shared_ptr<InData>&)
{
   const std::vector<std::shared_ptr<Handler>> handlers{ std::make_shared<TrezorPostHandler>
      (logger_, endpoint_) };
   return std::make_shared<bs::WorkerImpl>(handlers);
}
#endif

void bs::hww::JadeDevice::operationFailed(const std::string& reason)
{
   releaseConnection();
   //cb_->operationFailed(features_->device_id(), reason);
}

void JadeDevice::releaseConnection()
{
}

DeviceKey JadeDevice::key() const
{
   std::string walletId;
   std::string status;
   if (!xpubRoot_.empty()) {
      walletId = bs::core::wallet::computeID(xpubRoot_).toBinStr();
   }
   else {
      status = "not inited";
   }
/*   return {features_->label(), features_->device_id(), features_->vendor()
      , walletId, status, type() };*/
   return {};
}

DeviceType JadeDevice::type() const
{
   return DeviceType::HWJade;
}

void JadeDevice::init()
{
   logger_->debug("[JadeDevice::init] start");
}

void JadeDevice::getPublicKeys()
{
   awaitingWalletInfo_ = {};
   // General data
   awaitingWalletInfo_.type = bs::wallet::HardwareEncKey::WalletType::Trezor;
/*   awaitingWalletInfo_.label = features_->label();
   awaitingWalletInfo_.deviceId = features_->device_id();
   awaitingWalletInfo_.vendor = features_->vendor();
   awaitingWalletInfo_.xpubRoot = xpubRoot_.toBinStr();*/

}

void JadeDevice::setMatrixPin(const SecureBinaryData& pin)
{
   logger_->debug("[JadeDevice::setMatrixPin]");
}

void JadeDevice::setPassword(const SecureBinaryData& password, bool enterOnDevice)
{
   logger_->debug("[JadeDevice::setPassword]");
}

void JadeDevice::cancel()
{
   logger_->debug("[JadeDevice] cancel previous operation");
}

void JadeDevice::clearSession()
{
   logger_->debug("[JadeDevice] cancel session");
}


void JadeDevice::signTX(const bs::core::wallet::TXSignRequest &reqTX)
{
   logger_->debug("[JadeDevice::signTX]");
}

void JadeDevice::retrieveXPubRoot()
{
   logger_->debug("[JadeDevice::retrieveXPubRoot]");
}

void JadeDevice::reset()
{
   awaitingSignedTX_.clear();
   awaitingWalletInfo_ = {};
}
