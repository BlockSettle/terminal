#ifndef __REGTEST_CONTROLLER_H__
#define __REGTEST_CONTROLLER_H__

#include <QObject>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include "SocketObject.h"
#include "StringSockets.h"
#include "Address.h"


namespace spdlog {
   class logger;
}


class RegtestController : public QObject
{
   Q_OBJECT

public:
   RegtestController(const std::string &host, unsigned int port, const std::string &authCookie);

   bool GenerateBlocks(unsigned int numBlocks);
   QString SendTo(double amount, const bs::Address &);
   double GetBalance();
   QString Decode(const QString &hexTx);
   bool SendTx(const QString &hexTx);

private:
   QJsonObject RPC(const QString &method, const QVariantList &params);
   QVariantMap RPCMap(const QString &method, const QVariantList &params);
   QVariantList RPCList(const QString &method, const QVariantList &params);

private:
   std::unique_ptr<HttpSocket>   socket_;
};

#endif // __REGTEST_CONTROLLER_H__
