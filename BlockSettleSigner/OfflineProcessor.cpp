#include <QFile>
#include <QFileInfo>
#include <spdlog/spdlog.h>
#include "OfflineProcessor.h"
#include "OfflineSigner.h"
#include "WalletsManager.h"

#include "signer.pb.h"

using namespace Blocksettle;

Q_DECLARE_METATYPE(SecureBinaryData)


OfflineProcessor::OfflineProcessor(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<WalletsManager> &walletsMgr, const cbPassword &cb)
   : logger_(logger), walletsMgr_(walletsMgr), cbPassword_(cb)
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
      logger_->error("File {} doesn't exist", file.toStdString());
      emit signFailure();
      return;
   }
   if (!f.open(QIODevice::ReadOnly)) {
      logger_->error("Failed to open {} for reading", file.toStdString());
      emit signFailure();
      return;
   }

   const auto &data = f.readAll().toStdString();
   if (data.empty()) {
      logger_->error("File {} contains no data", file.toStdString());
      emit signFailure();
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
         logger_->error("File {} doesn't exist", file.toStdString());
         emit signFailure();
         return -1;
      }
   }
   if (!f->open(QIODevice::ReadOnly)) {
      logger_->error("Failed to open {} for reading", file.toStdString());
      emit signFailure();
      return -1;
   }

   const auto &data = f->readAll().toStdString();
   if (data.empty()) {
      logger_->error("File {} contains no data", file.toStdString());
      emit signFailure();
      return -1;
   }

   const auto &parsedReqs = ParseOfflineTXFile(data);
   if (parsedReqs.empty()) {
      logger_->error("File {} contains no TX sign requests", file.toStdString());
      emit signFailure();
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
      logger_->error("Failed to find sign request with ID {}", reqId);
      emit signFailure();
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
            const auto &wallet = walletsMgr_->GetWalletByAddress(address);
            result += tr("%1Input%2: %3 from %4@%5\n").arg(prefix).arg(inputIdx)
               .arg(displayXbt(utxo.getValue())).arg(address.display())
               .arg(wallet ? QString::fromStdString(wallet->GetWalletName()) : tr("<External>"));
         }
         for (int i = 0; i < req.request.recipients.size(); i++) {
            const auto &recipient = req.request.recipients[i];
            const auto recipIdx = (req.request.recipients.size() == 1) ? tr("") : tr("%1").arg(QString::number(i+1));
            const auto address = bs::Address::fromRecipient(recipient);
            const auto &wallet = walletsMgr_->GetWalletByAddress(address);
            result += tr("%1Output%2: %3 to %4@%5\n").arg(prefix).arg(recipIdx)
               .arg(displayXbt(recipient->getValue())).arg(address.display())
               .arg(wallet ? QString::fromStdString(wallet->GetWalletName()) : tr("<External>"));
         }
         if (req.request.change.value) {
            const auto &wallet = walletsMgr_->GetWalletByAddress(req.request.change.address);
            result += tr("%1Change: %2 to %3@%4\n").arg(prefix).arg(displayXbt(req.request.change.value))
               .arg(req.request.change.address.display())
               .arg(wallet ? QString::fromStdString(wallet->GetWalletName()) : tr("<External>"));
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

void OfflineProcessor::ProcessSignTX(const bs::wallet::TXSignRequest &txReq, const QString &reqFN)
{
   const auto &wallet = walletsMgr_->GetWalletById(txReq.walletId);
   if (!wallet) {
      logger_->error("Failed to find wallet with ID {}", txReq.walletId);
      emit signFailure();
      return;
   }

   SecureBinaryData password;
   if (wallet->encryptionType() != bs::wallet::EncryptionType::Unencrypted) {
      if (cbPassword_) {
         password = cbPassword_(wallet);
         if (password.isNull()) {
            logger_->error("Empty password for encrypted wallet {}", wallet->GetWalletName());
            emit signFailure();
            return;
         }
      }
      else {
         pendingReqs_[wallet->GetWalletId()].push_back({txReq, wallet, reqFN});
         if (pendingReqs_[wallet->GetWalletId()].size() == 1) {
            emit requestPassword(txReq);
         }
         return;
      }
   }
   SignTxRequest(txReq, reqFN, wallet, password);
}

void OfflineProcessor::SignTxRequest(const bs::wallet::TXSignRequest &txReq, const QString &reqFN
   , const std::shared_ptr<bs::Wallet> &wallet, const SecureBinaryData &password)
{
   try {
      const auto signedTX = wallet->SignTXRequest(txReq, password);

      QFileInfo fi(reqFN);
      QString outputFN = fi.path() + QLatin1String("/") + fi.baseName() + QLatin1String("_signed.bin");
      QFile f(outputFN);
      if (f.exists()) {
         logger_->error("File {} already exists", outputFN.toStdString());
         emit signFailure();
         return;
      }
      if (!f.open(QIODevice::WriteOnly)) {
         logger_->error("Failed to open {} for writing", outputFN.toStdString());
         emit signFailure();
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
      if (f.write(data) == data.size()) {
         logger_->info("Created signed TX response file in {}", outputFN.toStdString());
         // remove original request file?
      }
      else {
         logger_->error("Failed to write TX response data to {}", outputFN.toStdString());
      }
   }
   catch (const std::exception &e) {
      logger_->error("Failed to sign TX request: {}", e.what());
      emit signFailure();
   }
   emit signSuccess();
}

void OfflineProcessor::passwordEntered(const std::string &walletId, const SecureBinaryData &password)
{
   const auto &reqsIt = pendingReqs_.find(walletId);
   if (reqsIt == pendingReqs_.end()) {
      return;
   }
   logger_->debug("Signing {} pending request[s] on password receive", reqsIt->second.size());
   for (const auto &req : reqsIt->second) {
      SignTxRequest(req.request, req.requestFile, req.wallet, password);
   }
   pendingReqs_.erase(reqsIt);
}
