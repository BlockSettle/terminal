#include "ChatProtocol/ConnectionLogic.h"
#include "chat.pb.h"

namespace Chat
{

   ConnectionLogic::ConnectionLogic(const LoggerPtr& loggerPtr, QObject* parent /* = nullptr */) 
      : loggerPtr_(loggerPtr), QObject(parent)
   {

   }

   void ConnectionLogic::onDataReceived(const std::string&)
   {

   }

   void ConnectionLogic::onConnected(void)
   {

   }

   void ConnectionLogic::onDisconnected(void)
   {

   }

   void ConnectionLogic::onError(DataConnectionListener::DataConnectionError)
   {

   }
}