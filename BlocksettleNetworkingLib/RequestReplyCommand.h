#ifndef __REQUEST_REPLY_COMMAND_H__
#define __REQUEST_REPLY_COMMAND_H__

#include "DataConnectionListener.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace spdlog {
   class logger;
};

class ConnectionManager;
class DataConnection;
class ManualResetEvent;
// RequestReplyCommand - use it when you do not need to have opened connection
//                       and all you need is just send request and get reply
//                      connection could be closed after request received
class RequestReplyCommand : public DataConnectionListener
{
public:
   using data_callback_type = std::function<bool (const std::string&)>;
   using error_callback_type = std::function<void (const std::string&)>;

public:
   // name - only for debugging purposes
   RequestReplyCommand(const std::string& name
      , const std::shared_ptr<DataConnection>& connection
      , const std::shared_ptr<spdlog::logger>& logger);

   ~RequestReplyCommand() noexcept override;

   RequestReplyCommand(const RequestReplyCommand&) = delete;
   RequestReplyCommand& operator = (const RequestReplyCommand&) = delete;

   RequestReplyCommand(RequestReplyCommand&&) = delete;
   RequestReplyCommand& operator = (RequestReplyCommand&&) = delete;

   void SetReplyCallback(const data_callback_type& callback);
   void SetErrorCallback(const error_callback_type& callback);

   void CleanupCallbacks();

   std::string GetName() const { return name_; }

   bool ExecuteRequest(const std::string& host, const std::string& port
      , const std::string& data, bool executeOnConnect = false);
   bool WaitForRequestedProcessed(std::chrono::milliseconds period);

   bool GetExecutionResult() const { return result_; }
   void DropResult() { dropResult_ = true; }

   void resetConnection() { connection_.reset(); }

public:
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

private:
   const std::string name_;

   std::shared_ptr<DataConnection>  connection_;
   std::shared_ptr<spdlog::logger>  logger_;

   std::string                      requestData_;

   data_callback_type   replyCallback_;
   error_callback_type  errorCallback_;
   std::atomic_bool     dropResult_{false};

   bool replyReceived_{false};
   bool result_{false};
   bool executeOnConnect_{false};

   std::shared_ptr<ManualResetEvent> requestCompleted_;
};

#endif // __REQUEST_REPLY_COMMAND_H__
