#ifndef CELER_CLIENT_TCP_H
#define CELER_CLIENT_TCP_H

#include "CelerClient.h"

class CelerClientListener;
class ConnectionManager;

class CelerClientTcp : public CelerClient
{
public:
   CelerClientTcp(const std::shared_ptr<ConnectionManager>& connectionManager, bool userIdRequired = true);
   ~CelerClientTcp() override;


   bool LoginToServer(const std::string& hostname, const std::string& port
                      , const std::string& login, const std::string& password);

   void CloseConnection() override;

private:
   void onSendData(CelerAPI::CelerMessageType messageType, const std::string &data);

   friend class CelerClientListener;

   std::shared_ptr<ConnectionManager> connectionManager_;
   // Declare listener before connection_ (it should be destoyed after connection)
   std::unique_ptr<CelerClientListener> listener_;
   std::shared_ptr<DataConnection> connection_;
};

#endif
