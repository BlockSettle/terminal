#include "RegtestController.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonParseError>


RegtestController::RegtestController(const std::string &host, unsigned int port, const std::string &authCookie)
   : QObject()
{
   socket_ = make_unique<HttpSocket>(BinarySocket(host, std::to_string(port)));
   socket_->resetHeaders();   // base64-encoded auth cookie is from ~/.bitcoin/regtest/.cookie (snpriv1 in our case)
   socket_->addHeader("Authorization: Basic " + authCookie);
}

bool RegtestController::GenerateBlocks(unsigned int numBlocks)
{
   const auto result = RPCList(QLatin1String("generate"), { numBlocks });
   return (result.size() == numBlocks);
}

QString RegtestController::SendTo(double amount, const bs::Address &addr)
{
   const auto objReply = RPC(QLatin1String("sendtoaddress"), { addr.display(), amount });
   const auto itResult = objReply.find(QLatin1String("result"));
   if (itResult != objReply.end()) {
      return itResult.value().toString();
   }
   return QString();
}

double RegtestController::GetBalance()
{
   const auto result = RPCMap(QLatin1String("getinfo"), {});
   return result[QLatin1String("balance")].toDouble();
}

QString RegtestController::Decode(const QString &hexTx)
{
   const auto objReply = RPC(QLatin1String("decoderawtransaction"), { hexTx });
   const auto itResult = objReply.find(QLatin1String("result"));
   if (itResult != objReply.end()) {
      QJsonDocument doc(itResult.value().toObject());
      return QString::fromLatin1(doc.toJson(QJsonDocument::Compact));
   }
   return QString();
}

bool RegtestController::SendTx(const QString &hexTx)
{
   const auto objReply = RPC(QLatin1String("sendrawtransaction"), { hexTx });
   const auto itResult = objReply.find(QLatin1String("result"));
   if ((itResult != objReply.end()) && !itResult.value().toString().isEmpty()) {
      return true;
   }
   return false;
}

QJsonObject RegtestController::RPC(const QString &method, const QVariantList &params)
{
   if (!socket_->testConnection()) {
      qDebug() << "connection test failed";
      return {};
   }
   QJsonObject objReq;
   objReq[QLatin1String("method")] = method;
   objReq[QLatin1String("params")] = QJsonArray::fromVariantList(params);
   const auto reqData = QJsonDocument(objReq).toJson(QJsonDocument::Compact).toStdString();
   try {
      const auto &response = socket_->writeAndRead(reqData);
      QJsonParseError errJson;
      const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response), &errJson);
      if (errJson.error != QJsonParseError::NoError) {
         qDebug() << "parse error:" << errJson.errorString();
         return {};
      }
      return doc.object();
   }
   catch (const SocketError &e) {
      qDebug() << "socket error:" << e.what();
   }
   return {};
}

QVariantMap RegtestController::RPCMap(const QString &method, const QVariantList &params)
{
   const auto objReply = RPC(method, params);
   const auto itResult = objReply.find(QLatin1String("result"));
   if ((itResult != objReply.end()) && itResult.value().isObject()) {
      return itResult.value().toObject().toVariantMap();
   }
   return {};
}

QVariantList RegtestController::RPCList(const QString &method, const QVariantList &params)
{
   const auto objReply = RPC(method, params);
   const auto itResult = objReply.find(QLatin1String("result"));
   if ((itResult != objReply.end()) && itResult.value().isArray()) {
      return itResult.value().toArray().toVariantList();
   }
   return {};
}
