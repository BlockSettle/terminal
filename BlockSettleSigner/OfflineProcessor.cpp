#include <QFile>
#include <QFileInfo>
#include <spdlog/spdlog.h>
#include "make_unique.h"
#include "OfflineProcessor.h"
#include "OfflineSigner.h"
#include "SignerAdapter.h"
#include "Wallets/SyncWalletsManager.h"

#include "signer.pb.h"

using namespace Blocksettle;

Q_DECLARE_METATYPE(SecureBinaryData)


OfflineProcessor::OfflineProcessor(const std::shared_ptr<spdlog::logger> &logger
   , SignerAdapter *adapter, const CbPassword &cb)
   : logger_(logger), adapter_(adapter), cbPassword_(cb)
{
   qRegisterMetaType<SecureBinaryData>();
}

void OfflineProcessor::ProcessFiles(const QStringList &files)
{
   for (const auto &file : files) {
      processFile(file);
   }
}

void OfflineProcessor::processFile(const QString &file)
{
   logger_->debug("Processing file {}...", file.toStdString());
   QFile f(file);
   if (!f.exists()) {
      sendSignFailure(tr("File %1 doesn't exist").arg(file));
      return;
   }

   if (!f.open(QIODevice::ReadOnly)) {
      sendSignFailure(tr("Failed to open %1 for reading").arg(file));
      return;
   }

   const auto &data = f.readAll().toStdString();
   if (data.empty()) {
      sendSignFailure(tr("File %1 contains no data").arg(file));
      return;
   }

   const auto &txReqList = ParseOfflineTXFile(data);
   if (txReqList.empty()) {
      logger_->error("File {} contains no TX sign requests", file.toStdString());
   }
   else {
      for (const auto &txReq : txReqList) {
         ProcessSignTX(txReq, file);
      }
   }
}

int OfflineProcessor::parseFile(QString file)
{
   logger_->debug("Processing file {}...", file.toStdString());
   auto f = make_unique<QFile>(file);
   if (!f->exists()) {
#if !defined (Q_OS_WIN)
      if (!file.startsWith(QLatin1Char('/'))) {
         file = QLatin1String("/") + file;
         f = make_unique<QFile>(file);
      }
#endif
      if (!f->exists()) {
         sendSignFailure(tr("File %1 doesn't exist").arg(file));
         return -1;
      }
   }
   if (!f->open(QIODevice::ReadOnly)) {
      sendSignFailure(tr("Failed to open %1 for reading").arg(file));
      return -1;
   }

   const auto &data = f->readAll().toStdString();
   if (data.empty()) {
      sendSignFailure(tr("File %1 contains no data").arg(file));
      return -1;
   }

   const auto &parsedReqs = ParseOfflineTXFile(data);
   if (parsedReqs.empty()) {
      sendSignFailure(tr("File %1 contains no TX sign requests").arg(file));
      return -1;
   }

   std::vector<SignData> reqList;
   for (const auto &req : parsedReqs) {
      reqList.push_back(SignData{req, nullptr, file});
   }

   auto reqId = ++reqSeqNum_;

   for (const auto &req : reqList) {
      if (!req.request.prevStates.empty()) {    // Special case for already signed request
         reqId = -reqId;                        // We show it, but don't allow confirm button
         break;
      }
   }

   parsedReqs_[reqId] = reqList;
   return reqId;
}

void OfflineProcessor::processRequest(int reqId)
{
   const auto reqIt = parsedReqs_.find(reqId);
   if (reqIt == parsedReqs_.end()) {
      sendSignFailure(tr("Failed to find sign request with ID %1").arg(reqId));
   }
   else {
      for (const auto &req : reqIt->second) {
         ProcessSignTX(req.request, req.requestFile);
      }
   }
   parsedReqs_.erase(reqIt);
}

static QString displayXbt(const uint64_t value)
{
   return QString::number(value / BTCNumericTypes::BalanceDivider, 'f', 8);
}

QString OfflineProcessor::parsedText(int reqId) const
{
   auto reqIt = parsedReqs_.find(reqId);
   if (reqIt == parsedReqs_.end()) {
      return tr("Sign request with ID %1 not found").arg(QString::number(reqId));
   }
   const bool isSingleReq = (reqIt->second.size() == 1);
   const auto walletsMgr = adapter_->getWalletsManager();
   if (!walletsMgr) {
      return tr("No wallets manager found");
   }
   size_t txCounter = 1;
   QString result = isSingleReq ? tr("") : tr("%1 transactions in request:\n").arg(QString::number(reqIt->second.size()));
   for (const auto &req : reqIt->second) {
      const auto prefix = isSingleReq ? tr("") : tr("#%1: ").arg(QString::number(txCounter));
      if (!req.request.prevStates.empty()) {
         result += tr("%1Already signed binary transaction").arg(prefix);
      }
      else {
         for (int i = 0; i < req.request.inputs.size(); i++) {
            const auto &utxo = req.request.inputs[i];
            const auto inputIdx = (req.request.inputs.size() == 1) ? tr("") : tr("%1").arg(QString::number(i+1));
            const auto address = bs::Address::fromUTXO(utxo);
            const auto &wallet = walletsMgr->getWalletByAddress(address);
            result += tr("%1Input%2: %3 from %4@%5\n").arg(prefix).arg(inputIdx)
               .arg(displayXbt(utxo.getValue())).arg(QString::fromStdString(address.display()))
               .arg(wallet ? QString::fromStdString(wallet->name()) : tr("<External>"));
         }
         for (int i = 0; i < req.request.recipients.size(); i++) {
            const auto &recipient = req.request.recipients[i];
            const auto recipIdx = (req.request.recipients.size() == 1) ? tr("") : tr("%1").arg(QString::number(i+1));
            const auto address = bs::Address::fromRecipient(recipient);
            const auto &wallet = walletsMgr->getWalletByAddress(address);
            result += tr("%1Output%2: %3 to %4@%5\n").arg(prefix).arg(recipIdx)
               .arg(displayXbt(recipient->getValue())).arg(QString::fromStdString(address.display()))
               .arg(wallet ? QString::fromStdString(wallet->name()) : tr("<External>"));
         }
         if (req.request.change.value) {
            const auto &wallet = walletsMgr->getWalletByAddress(req.request.change.address);
            result += tr("%1Change: %2 to %3@%4\n").arg(prefix).arg(displayXbt(req.request.change.value))
               .arg(QString::fromStdString(req.request.change.address.display()))
               .arg(wallet ? QString::fromStdString(wallet->name()) : tr("<External>"));
         }
         result += tr("%1Fee: %2%3").arg(prefix).arg(displayXbt(req.request.fee))
            .arg((txCounter == reqIt->second.size()) ? QString() : QLatin1String("\n"));
      }
      txCounter++;
   }
   return result;
}

void OfflineProcessor::removeSignReq(int reqId)
{
   auto reqIt = parsedReqs_.find(reqId);
   if (reqIt == parsedReqs_.end()) {
      return;
   }
   parsedReqs_.erase(reqId);
}

void OfflineProcessor::ProcessSignTX(const bs::core::wallet::TXSignRequest &txReq, const QString &reqFileName)
{
   if (txReq.walletId.empty()) {
      sendSignFailure(tr("Invalid sign transaction request"));
      return;
   }

   const auto walletsMgr = adapter_->getWalletsManager();
   const auto &wallet = walletsMgr->getWalletById(txReq.walletId);
   if (!wallet) {
      sendSignFailure(tr("Failed to find wallet with ID %1").arg(QString::fromStdString(txReq.walletId)));
      return;
   }

   SecureBinaryData password;
   if (!wallet->encryptionTypes().empty()) {
      if (cbPassword_) {
         password = cbPassword_(wallet);
         if (password.isNull()) {
            sendSignFailure(tr("Empty password for encrypted wallet %1").arg(QString::fromStdString(wallet->name())));
            return;
         }
      }
      else {
         pendingReqs_[wallet->walletId()].push_back({txReq, wallet, reqFileName});
         if (pendingReqs_[wallet->walletId()].size() == 1) {
            emit requestPassword(txReq);
         }
         return;
      }
   }
   SignTxRequest(txReq, reqFileName, password);
}

void OfflineProcessor::SignTxRequest(const bs::core::wallet::TXSignRequest &txReq, const QString &reqFN
   , const SecureBinaryData &password)
{
   const auto &cbSigned = [this, reqFN, txReq] (const BinaryData &signedTX) {
      if (signedTX.isNull()) {
         sendSignFailure(tr("Invalid sign request"));
         return;
      }
      QFileInfo fi(reqFN);
      QString outputFN = fi.path() + QLatin1String("/") + fi.baseName() + QLatin1String("_signed.bin");
      QFile f(outputFN);
      if (f.exists()) {
         sendSignFailure(tr("File %1 already exists").arg(outputFN));
         return;
      }
      if (!f.open(QIODevice::WriteOnly)) {
         sendSignFailure(tr("Failed to open %1 for writing").arg(outputFN));
         return;
      }

      Storage::Signer::SignedTX response;
      response.set_transaction(signedTX.toBinStr());
      response.set_comment(txReq.comment);

      Storage::Signer::File fileContainer;
      auto container = fileContainer.add_payload();
      container->set_type(Storage::Signer::SignedTXFileType);
      container->set_data(response.SerializeAsString());

      const auto data = QByteArray::fromStdString(fileContainer.SerializeAsString());
      if (f.write(data) != data.size()) {
         sendSignFailure(tr("Failed to write TX response data to %1").arg(outputFN));
         return;
      }

      logger_->info("Created signed TX response file in {}", outputFN.toStdString());
      // remove original request file?
      emit signSuccess(outputFN);
   };
   adapter_->signOfflineTxRequest(txReq, cbSigned);
}

void OfflineProcessor::sendSignFailure(const QString &errorMsg)
{
   SPDLOG_LOGGER_ERROR(logger_, "offline TX sign failed: {}", errorMsg.toStdString());
   emit signFailure(errorMsg);
}

void OfflineProcessor::passwordEntered(const std::string &walletId, const SecureBinaryData &password, bool cancelledByUser)
{
   const auto &reqsIt = pendingReqs_.find(walletId);
   if (reqsIt == pendingReqs_.end()) {
      return;
   }
   logger_->debug("Signing {} pending request[s] on password receive, cancelledByUser: {}"
      , reqsIt->second.size(), cancelledByUser);
   for (const auto &req : reqsIt->second) {
      if (!cancelledByUser) {
         SignTxRequest(req.request, req.requestFile, password);
      }
   }
   pendingReqs_.erase(reqsIt);
}
