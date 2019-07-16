#ifndef CELER_CLIENT_H
#define CELER_CLIENT_H

#include "BaseCelerClient.h"

class CelerClientListener;
class ConnectionManager;

class CelerClient : public BaseCelerClient
{
public:
   CelerClient(const std::shared_ptr<ConnectionManager>& connectionManager, bool userIdRequired = true);
   ~CelerClient() override;


   bool LoginToServer(const std::string& hostname, const std::string& port
                      , const std::string& login, const std::string& password);

   void CloseConnection() override;

protected:
   void onSendData(CelerAPI::CelerMessageType messageType, const std::string &data) override;

private:

   friend class CelerClientListener;

   std::shared_ptr<ConnectionManager> connectionManager_;
   // Declare listener before connection_ (it should be destoyed after connection)
   std::unique_ptr<CelerClientListener> listener_;
   std::shared_ptr<DataConnection> connection_;
};

#endif
