#ifndef CELER_CLIENT_PROXY_H
#define CELER_CLIENT_PROXY_H

#include "BaseCelerClient.h"
#include "BsClient.h"

class CelerClientProxy : public BaseCelerClient
{
public:
   CelerClientProxy(const std::shared_ptr<spdlog::logger> &logger, bool userIdRequired = true);
   ~CelerClientProxy() override;

   bool LoginToServer(BsClient *client, const std::string& login);
private:
   void onSendData(CelerAPI::CelerMessageType messageType, const std::string &data) override;

   BsClient *client_{};
};

#endif
