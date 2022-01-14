/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SignerAdapterContainer.h"
#include <spdlog/spdlog.h>
#include <QDataStream>
#include <QFile>
#include <QStandardPaths>
#include "DataConnection.h"
#include "DataConnectionListener.h"
#include "HeadlessApp.h"
#include "ProtobufHeadlessUtils.h"
#include "SignerInterfaceListener.h"
#include "Wallets/SyncWalletsManager.h"

#include "bs_signer.pb.h"

using namespace Blocksettle::Communication;

bs::signer::RequestId SignAdapterContainer::signTXRequest(const bs::core::wallet::TXSignRequest &txReq
   , const SecureBinaryData &password)
{
   signer::SignOfflineTxRequest request;
   request.set_password(password.toBinStr());
   *(request.mutable_tx_request()) = bs::signer::coreTxRequestToPb(txReq);

   return listener_->send(signer::SignOfflineTxRequestType, request.SerializeAsString());
}

bs::signer::RequestId SignAdapterContainer::DeleteHDRoot(const std::string &rootWalletId) {
   headless::DeleteHDWalletRequest request;
   request.set_rootwalletid(rootWalletId);
   const auto reqId = listener_->send(signer::DeleteHDWalletType, request.SerializeAsString());
   return reqId;
}

void SignAdapterContainer::syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb)
{
   const auto &cbWrap = [this, cb](std::vector<bs::sync::WalletInfo> wi) {
      woWallets_.clear();
      for (const auto &wallet : wi) {
         if (wallet.watchOnly && (wallet.format == bs::sync::WalletFormat::HD)) {
            woWallets_.insert(*wallet.ids.cbegin());
         }
      }
      if (cb) {
         cb(wi);
      }
   };
   const auto reqId = listener_->send(signer::SyncWalletInfoType, "");
   listener_->setWalletInfoCb(reqId, cbWrap);
}

void SignAdapterContainer::syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &cb)
{
   signer::SyncWalletRequest request;
   request.set_wallet_id(id);
   const auto reqId = listener_->send(signer::SyncHDWalletType, request.SerializeAsString());
   listener_->setHDWalletDataCb(reqId, cb);
}

void SignAdapterContainer::syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &cb)
{
   signer::SyncWalletRequest request;
   request.set_wallet_id(id);
   const auto reqId = listener_->send(signer::SyncWalletType, request.SerializeAsString());
   listener_->setWalletDataCb(reqId, cb);
}
