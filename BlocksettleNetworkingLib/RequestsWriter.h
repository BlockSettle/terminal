#ifndef __REQUESTS_WRITER_H__
#define __REQUESTS_WRITER_H__

#include <string>
#include <memory>

#include <QString>

namespace spdlog {
   class logger;
};

class RequestsWriter
{
public:
   RequestsWriter(const std::shared_ptr<spdlog::logger>& logger
      , const QString& requestsRoot);
   ~RequestsWriter() noexcept = default;

   RequestsWriter(const RequestsWriter&) = delete;
   RequestsWriter& operator = (const RequestsWriter&) = delete;

   RequestsWriter(RequestsWriter&&) = delete;
   RequestsWriter& operator = (RequestsWriter&&) = delete;

   bool SaveRequest(const std::string& request, const std::string& requestName = std::string{}) const;

private:
   QString GetCurrentRequestsDir() const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   QString                          requestsRoot_;
};

#endif // __REQUESTS_WRITER_H__