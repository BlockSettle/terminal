/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "jadeClient.h"
#include <curl/curl.h>
#include "hwdevicemanager.h"
#include "jadeDevice.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"

using namespace bs::hww;

JadeClient::JadeClient(const std::shared_ptr<spdlog::logger>& logger
   , bool testNet, DeviceCallbacks* cb)
   : bs::WorkerPool(1, 1), logger_(logger), cb_(cb), testNet_(testNet)
{}

void JadeClient::initConnection()
{
   logger_->info("[JadeClient::initConnection]");
}

std::vector<DeviceKey> JadeClient::deviceKeys() const
{
   std::vector<DeviceKey> result;
   return result;
}

std::shared_ptr<JadeDevice> JadeClient::getDevice(const std::string& deviceId)
{
   return nullptr;
}

#if 0
std::shared_ptr<bs::Worker> JadeClient::worker(const std::shared_ptr<InData>&)
{
   const std::vector<std::shared_ptr<Handler>> handlers{ std::make_shared<TrezorPostHandler>
      (logger_, trezorEndPoint_) };
   return std::make_shared<bs::WorkerImpl>(handlers);
}
#endif

void JadeClient::scanDevices()
{
   logger_->info("[JadeClient::scanDevices]");
}
