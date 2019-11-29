/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __GENOA_CONNECTION_H__
#define __GENOA_CONNECTION_H__

#include <spdlog/spdlog.h>

#include <string>
#include <memory>

template<class _S>
class GenoaConnection : public _S
{
public:
   GenoaConnection(const std::shared_ptr<spdlog::logger>& logger)
      : _S(logger)
   {}
   GenoaConnection(const std::shared_ptr<spdlog::logger>& logger, bool monitored)
      : _S(logger, monitored)
   {}

   ~GenoaConnection() noexcept override = default;

   GenoaConnection(const GenoaConnection&) = delete;
   GenoaConnection& operator = (const GenoaConnection&) = delete;

   GenoaConnection(GenoaConnection&&) = delete;
   GenoaConnection& operator = (GenoaConnection&&) = delete;

public:
   bool send(const std::string& data) override
   {
      std::string message = data + marker;
      return _S::sendRawData(message);
   }

protected:
   void onRawDataReceived(const std::string& rawData) override
   {
      pendingData_.append(rawData);
      ProcessIncomingData();
   }

private:
   void ProcessIncomingData() {
      size_t position;
      while ( (position = pendingData_.find(marker)) != std::string::npos) {
         std::string message = pendingData_.substr(0, position);
         pendingData_ = pendingData_.substr(position + marker.size());
         _S::notifyOnData(message);
      }
   }

private:
   std::string pendingData_;
   const std::string marker = "\r\n\r\n";
};

#endif // __GENOA_CONNECTION_H__
