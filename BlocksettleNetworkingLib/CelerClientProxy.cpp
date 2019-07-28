#include "CelerClientProxy.h"

#include <spdlog/spdlog.h>

CelerClientProxy::CelerClientProxy(const std::shared_ptr<spdlog::logger> &logger, bool userIdRequired)
   : BaseCelerClient (logger, userIdRequired)
{
}

CelerClientProxy::~CelerClientProxy() = default;

bool CelerClientProxy::LoginToServer(BsClient *client, const std::string &login, const std::string &email)
{
   client_ = client;

   connect(client_, &BsClient::celerRecv, this, [this](CelerAPI::CelerMessageType messageType, const std::string &data) {
      recvData(messageType, data);
   });

   connect(client_, &QObject::destroyed, this, [this] {
      client_ = nullptr;
   });

   // Password will be replaced by BsProxy
   bool result = SendLogin(login, email, "");
   return result;
}

void CelerClientProxy::onSendData(CelerAPI::CelerMessageType messageType, const std::string &data)
{
   if (!client_) {
      SPDLOG_LOGGER_ERROR(logger_, "BsClient is not valid");
      return;
   }

   client_->celerSend(messageType, data);
}
