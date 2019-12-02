/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CC_PUB_CONNECTION_H__
#define __CC_PUB_CONNECTION_H__

#include "CommonTypes.h"

#include <QObject>

#include <memory>
#include <string>

#include "ZMQ_BIP15X_Helpers.h"

namespace spdlog {
   class logger;
}

class ConnectionManager;
class RequestReplyCommand;

class CCPubConnection : public QObject
{
Q_OBJECT

public:
   CCPubConnection(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<ConnectionManager> &
      , const ZmqBipNewKeyCb &cb = nullptr);
   ~CCPubConnection() noexcept override = default;

   CCPubConnection(const CCPubConnection&) = delete;
   CCPubConnection& operator = (const CCPubConnection&) = delete;

   CCPubConnection(CCPubConnection&&) = delete;
   CCPubConnection& operator = (CCPubConnection&&) = delete;

   bool LoadCCDefinitionsFromPub();

protected:
   virtual std::string GetPuBHost() const = 0;
   virtual std::string GetPuBPort() const = 0;
   virtual std::string GetPuBKey() const = 0;

   virtual bool IsTestNet() const = 0;

   bool SubmitRequestToPB(const std::string& name, const std::string& data);

   virtual void ProcessGenAddressesResponse(const std::string& response, const std::string &sig) = 0;
   virtual void ProcessSubmitAddrResponse(const std::string& response) = 0;

private:
   void OnDataReceived(const std::string& data);

   void ProcessErrorResponse(const std::string& responseString) const;

protected:
   std::shared_ptr<spdlog::logger>  logger_;
   unsigned int                     currentRev_ = 0;

private:
   std::shared_ptr<ConnectionManager>     connectionManager_;
   ZmqBipNewKeyCb      cbApproveConn_ = nullptr;
   std::shared_ptr<RequestReplyCommand>   cmdPuB_;
};

#endif // __CC_PUB_CONNECTION_H__
