#include "ChatProtocol/ClientDBLogic.h"

namespace Chat
{

   ClientDBLogic::ClientDBLogic(QObject* parent /* = nullptr */) : DatabaseExecutor(parent)
   {

   }

   void ClientDBLogic::Init(const Chat::LoggerPtr& loggerPtr, const ApplicationSettingsPtr& appSettings)
   {

   }
}