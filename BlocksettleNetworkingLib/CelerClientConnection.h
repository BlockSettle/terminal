/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_CLIENT_CONNECTION_H__
#define __CELER_CLIENT_CONNECTION_H__

#include <spdlog/spdlog.h>

#include <string>
#include <memory>

template<class _S>
class CelerClientConnection : public _S
{
public:
   CelerClientConnection(const std::shared_ptr<spdlog::logger>& logger)
      : _S(logger)
      , pendingDataSize_(0)
   {}

   ~CelerClientConnection() noexcept = default;

   CelerClientConnection(const CelerClientConnection&) = delete;
   CelerClientConnection& operator = (const CelerClientConnection&) = delete;

   CelerClientConnection(CelerClientConnection&&) = delete;
   CelerClientConnection& operator = (CelerClientConnection&&) = delete;

public:
   bool send(const std::string& data) override
   {
      auto size = data.size();
      char sizeBuffer[4];
      uint32_t bufferLength = 0;

      while(true) {
         if (bufferLength == 4) {
            return false;
         }

         sizeBuffer[bufferLength] = size & 0x7f;
         size = size >> 7;
         if (size == 0) {
            break;
         }

         sizeBuffer[bufferLength] |= 0x80;
         ++bufferLength;
      }
      return _S::sendRawData(std::string((char*)sizeBuffer, bufferLength+1) + data);
   }

protected:
   void onRawDataReceived(const std::string& rawData) override
   {
      pendingData_.append(rawData);

      while (!pendingData_.empty()) {
         if (pendingDataSize_ == 0) {
            const char *sizeBuffer = pendingData_.c_str();

            int offset = 0;
            int sizeBytesCount = 0;

            while (true) {
               if (offset >= pendingData_.size()) {
                  _S::logger_->error("[CelerClientConnection] not all size bytes received");
                  return;
               }

               if (sizeBuffer[offset] & 0x80) {
                  if (offset == 3) {
                     // we do not expect more than 4 bytes for size
                     _S::logger_->error("[CelerClientConnection] could not decode size");
                     return;
                  }

                  offset += 1;
               } else {
                  break;
               }
            }

            sizeBytesCount = offset + 1;

            while (offset >= 0) {
               pendingDataSize_ = pendingDataSize_ << 7;
               pendingDataSize_ |= (sizeBuffer[offset] & 0x7f);
               offset -= 1;
            }

            pendingData_ = pendingData_.substr(sizeBytesCount);
         }

         if (pendingDataSize_ > pendingData_.size()) {
            break;
         }

         _S::notifyOnData(pendingData_.substr(0, pendingDataSize_));
         pendingData_ = pendingData_.substr(pendingDataSize_);
         pendingDataSize_ = 0;
      }
   }

private:
   size_t      pendingDataSize_;
   std::string pendingData_;
};

#endif // __CELER_CLIENT_CONNECTION_H__
