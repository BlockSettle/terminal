/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "jadeClient.h"
#include "hwdevicemanager.h"
#include "jadeDevice.h"
#include "SystemFileUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"

namespace {
#ifdef Q_OS_WINDOWS
   const std::vector<std::string> kSerialPorts {"COM1", "COM2", "COM3", "COM4"
      , "COM5", "COM6", "COM7", "COM8", "COM9", "COM10", "COM11", "COM12"};
#elif Q_OS_MACOS
   const std::vector<std::string> kSerialPorts{ "/dev/tty.usbserial*"
      , "/dev/cu.usbserial", "/dev/cu.Bluetooth-Modem"};
#else
   const std::vector<std::string> kSerialPorts{ "/dev/ttyS*", "/dev/ttyACM*"
      , "/dev/ttyUSB*"};
#endif
}

using namespace bs::hww;
using json = nlohmann::json;

JadeClient::JadeClient(const std::shared_ptr<spdlog::logger>& logger
   , bool testNet, DeviceCallbacks* cb)
   : bs::WorkerPool(1, 1), logger_(logger), cb_(cb), testNet_(testNet)
{}

void JadeClient::initConnection()
{
   logger_->info("[JadeClient::initConnection]");
}

std::vector<DeviceKey> JadeClient::deviceKeys() const
{
   std::vector<DeviceKey> result;
   return result;
}

std::shared_ptr<JadeDevice> JadeClient::getDevice(const std::string& deviceId)
{
   return nullptr;
}

std::shared_ptr<bs::Worker> JadeClient::worker(const std::shared_ptr<InData>&)
{
   const std::vector<std::shared_ptr<Handler>> handlers{std::make_shared<JadeHandler>(logger_) };
   return std::make_shared<bs::WorkerImpl>(handlers);
}

void JadeClient::scanDevices()
{
   logger_->info("[JadeClient::scanDevices]");
   std::vector<std::string> serialPorts;
   for (const auto& fn : kSerialPorts) {
      if (fn.find('*') == std::string::npos) {
#ifdef Q_OS_WINDOWS
         serialPorts.push_back(fn);
#else
         if (SystemFileUtils::deviceExist(fn)) {
            serialPorts.push_back(fn);
         }
#endif
      }
      else {
         const auto& files = SystemFileUtils::readDir("/", fn);
         for (const auto& f : files) {
            if (SystemFileUtils::deviceExist(f)) {
               serialPorts.push_back(f);
            }
         }
      }
   }
   if (serialPorts.empty()) {
      cb_->scanningDone();
      return;
   }
   auto nbCalls = std::make_shared<int>((int)serialPorts.size());
   for (const auto& serial : serialPorts) {
      const auto& cb = [this, nbCalls](const std::shared_ptr<OutData>& out)
      {
         const auto& cbScanned = [this, nbCalls]
         {
            (*nbCalls)--;
            if (*nbCalls <= 0) {
               logger_->debug("[JadeClient::scanDevices] all devices scanned");
               cb_->scanningDone();
            }
         };

         const auto& data = std::dynamic_pointer_cast<JadeOut>(out);
         if (!data) {
            logger_->error("[JadeClient::scanDevices] invalid callback data");
            cbScanned();
            return;
         }
         if (!data->response.empty()) {
            logger_->debug("[JadeClient::scanDevices] response: {}", data->response.dump());
         }
         cbScanned();
      };
      const auto request = std::make_shared<JadeIn>(serial, json{ {"method", "get_version_info"}});
      processQueued(request, cb);
   }
}

bs::hww::JadeHandler::JadeHandler(const std::shared_ptr<spdlog::logger>& logger)
   : logger_(logger)
{}

#ifdef Q_OS_WINDOWS
static std::string lastErrorAsString()
{
   DWORD errorMessageID = ::GetLastError();
   if (errorMessageID == 0) {
      return {};
   }
   LPSTR messageBuffer = nullptr;

   //Ask Win32 to give us the string version of that message ID.
   //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
   size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

   const std::string message(messageBuffer, size);
   //Free the Win32's string's buffer.
   LocalFree(messageBuffer);
   return message;
}
#endif

std::shared_ptr<JadeOut> bs::hww::JadeHandler::processData(const std::shared_ptr<JadeIn>& in)
{
   auto out = std::make_shared<JadeOut>(in->serial);
#ifdef Q_OS_WINDOWS
   HANDLE hComm;

   hComm = CreateFileA(("\\\\.\\" + in->serial).c_str(),
      GENERIC_READ | GENERIC_WRITE, 0, // No Sharing
      NULL,                         // No Security
      OPEN_EXISTING,// Open existing port only
      0,            // Non Overlapped I/O
      NULL);        // Null for Comm Devices

   logger_->debug("[{}] {} handle = {:x}", __func__, in->serial, hComm);
   if (hComm == INVALID_HANDLE_VALUE) {
      out->error = "failed to open serial port " + in->serial + ": " + lastErrorAsString();
      logger_->debug("[{}] {}", __func__, out->error);
      return out;
   }
   json reqCopy = in->request;
   reqCopy["id"] = ++seqNo_;
   logger_->debug("[{}] writing {} to {}", __func__, reqCopy.dump(), in->serial);
   const auto& reqBytes = json::to_cbor(reqCopy);
   const std::string reqBuf(reqBytes.cbegin(), reqBytes.cend());

   DWORD nbWritten = 0;
   const bool rc = WriteFile(hComm, reqBuf.data(), reqBuf.size(), &nbWritten, NULL);
   if (!rc || (nbWritten != reqBuf.size())) {
      out->error = "failed to write " + std::to_string(reqBuf.size() - nbWritten)
         + " out of " + std::to_string(reqBuf.size()) + " bytes: " + lastErrorAsString();
      logger_->error("[{}] {}", __func__, out->error);
      CloseHandle(hComm);
      return out;
   }
   std::string readBuf;
   char buf[64];
   DWORD nbRead = 0;
   while (ReadFile(hComm, &buf, sizeof(buf), &nbRead, NULL)) {
      if (nbRead <= 0) {
         break;
      }
      logger_->debug("[{}] read {} bytes from {}", __func__, nbRead, in->serial);
      readBuf.append(std::string(buf, nbRead));
   }
   logger_->debug("[{}] {} bytes read from {}", __func__, readBuf.size(), in->serial);
   if (!readBuf.empty()) {
      out->response = json::from_cbor(readBuf.cbegin(), readBuf.cend());
   }
   CloseHandle(hComm);
#else
#endif
   return out;
}
