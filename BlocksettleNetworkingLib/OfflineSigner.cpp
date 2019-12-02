/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "Address.h"
#include "OfflineSigner.h"
#include "ProtobufHeadlessUtils.h"

#include "signer.pb.h"
#include "headless.pb.h"

#include <QFile>

using namespace Blocksettle;
using namespace Blocksettle::Communication;
using namespace bs::error;

std::vector<bs::core::wallet::TXSignRequest> bs::core::wallet::ParseOfflineTXFile(const std::string &data)
{
   try {
      Storage::Signer::File fileContainer;
      if (!fileContainer.ParseFromString(data)) {
         return {};
      }
      std::vector<bs::core::wallet::TXSignRequest> result;
      for (int i = 0; i < fileContainer.payload_size(); i++) {
         const auto container = fileContainer.payload(i);
         bs::core::wallet::TXSignRequest txReq;
         if (container.type() == Storage::Signer::RequestFileType) {

            headless::SignTxRequest tx;
            if (!tx.ParseFromString(container.data())) {
               continue;
            }

            txReq = bs::signer::pbTxRequestToCore(tx);
         }
         else if (container.type() == Storage::Signer::SignedTXFileType) {
            Storage::Signer::SignedTX tx;
            if (!tx.ParseFromString(container.data())) {
               continue;
            }
            txReq.prevStates.push_back(tx.transaction());
            txReq.comment = tx.comment();
         }
         else {
            continue;
         }
         result.push_back(txReq);
      }
      return result;
   } catch (...) {
      return {};
   }

}

ErrorCode bs::core::wallet::ExportTxToFile(const bs::core::wallet::TXSignRequest &txSignReq, const QString &fileNamePath)
{
   if (!txSignReq.isValid()) {
      return ErrorCode::TxInvalidRequest;
   }

   headless::SignTxRequest request = bs::signer::coreTxRequestToPb(txSignReq);

   Blocksettle::Storage::Signer::File fileContainer;
   auto container = fileContainer.add_payload();
   container->set_type(Blocksettle::Storage::Signer::RequestFileType);
   container->set_data(request.SerializeAsString());

   QFile f(fileNamePath);

   if (!f.open(QIODevice::WriteOnly)) {
      return ErrorCode::TxFailedToOpenRequestFile;
   }

   const auto data = QByteArray::fromStdString(fileContainer.SerializeAsString());
   if (f.write(data) == data.size()) {
      return ErrorCode::NoError;
   }
   else {
      return ErrorCode::TxFailedToWriteRequestFile;
   }
}

ErrorCode bs::core::wallet::ExportSignedTxToFile(const BinaryData &signedTx, const QString &fileNamePath, const std::string &comment)
{
   QFile f(fileNamePath);
   if (f.exists()) {
      return ErrorCode::TxRequestFileExist;
   }
   if (!f.open(QIODevice::WriteOnly)) {
      return ErrorCode::TxFailedToOpenRequestFile;
   }

   Storage::Signer::SignedTX response;
   response.set_transaction(signedTx.toBinStr());
   response.set_comment(comment);

   Storage::Signer::File fileContainer;
   auto container = fileContainer.add_payload();
   container->set_type(Storage::Signer::SignedTXFileType);
   container->set_data(response.SerializeAsString());

   const auto data = QByteArray::fromStdString(fileContainer.SerializeAsString());
   if (f.write(data) == data.size()) {
      return ErrorCode::NoError;
   }
   else {
      return ErrorCode::TxFailedToWriteRequestFile;
   }
}
