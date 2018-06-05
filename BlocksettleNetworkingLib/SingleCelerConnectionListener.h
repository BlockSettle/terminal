#ifndef __SINGLE_CELER_CONNECTION_LISTENER_H__
#define __SINGLE_CELER_CONNECTION_LISTENER_H__

#include "SingleConnectionServerListener.h"

#include "CelerMessageMapper.h"

#include <memory>

class ConnectionManager;

class SingleCelerConnectionListener : public SingleConnectionServerListener
{
public:
   SingleCelerConnectionListener(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::string& name, bool extraLogging = false);
   ~SingleCelerConnectionListener() noexcept override = default;

   SingleCelerConnectionListener(const SingleCelerConnectionListener&) = delete;
   SingleCelerConnectionListener& operator = (const SingleCelerConnectionListener&) = delete;

   SingleCelerConnectionListener(SingleCelerConnectionListener&&) = delete;
   SingleCelerConnectionListener& operator = (SingleCelerConnectionListener&&) = delete;

protected:
   bool ProcessDataFromClient(const std::string& data) override;

   virtual bool ProcessRequestDataFromClient(CelerAPI::CelerMessageType messageType
      , int64_t sequenceNumber, const std::string& data) = 0;
private:
   bool ProcessHeartBeat(int64_t sequenceNumber);
   bool ReturnHeartbeat(int64_t sequenceNumber);
};

#endif // __SINGLE_CELER_CONNECTION_LISTENER_H__
