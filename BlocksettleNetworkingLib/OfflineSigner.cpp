#include "Address.h"
#include "OfflineSigner.h"
#include "ProtobufHeadlessUtils.h"

#include "signer.pb.h"
#include "headless.pb.h"

#include <QFile>

using namespace Blocksettle;
using namespace Blocksettle::Communication;

std::vector<bs::core::wallet::TXSignRequest> bs::core::wallet::ParseOfflineTXFile(const std::string &data)
{
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
}

bs::error::ErrorCode bs::core::wallet::ExportTxToFile(const bs::core::wallet::TXSignRequest &txSignReq, const QString &fileNamePath)
{
   if (!txSignReq.isValid()) {
      return bs::error::ErrorCode::TxInvalidRequest;
   }

   headless::SignTxRequest request = bs::signer::coreTxRequestToPb(txSignReq);

   Blocksettle::Storage::Signer::File fileContainer;
   auto container = fileContainer.add_payload();
   container->set_type(Blocksettle::Storage::Signer::RequestFileType);
   container->set_data(request.SerializeAsString());

   QFile f(fileNamePath);

   if (!f.open(QIODevice::WriteOnly)) {
      return bs::error::ErrorCode::TxFailedToOpenRequestFile;
   }

   const auto data = QByteArray::fromStdString(fileContainer.SerializeAsString());
   if (f.write(data) == data.size()) {
      return bs::error::ErrorCode::NoError;
   }
   else {
      return bs::error::ErrorCode::TxFailedToWriteRequestFile;
   }
}
