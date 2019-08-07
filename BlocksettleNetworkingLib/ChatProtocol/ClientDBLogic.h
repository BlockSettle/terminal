#ifndef ClientDBLogic_h__
#define ClientDBLogic_h__

#include "ChatProtocol/DatabaseExecutor.h"

class QSqlDatabase;
class ApplicationSettings;

namespace Chat
{
   using ApplicationSettingsPtr = std::shared_ptr<ApplicationSettings>;

   class ClientDBLogic : public DatabaseExecutor
   {
      Q_OBJECT

   public:
      ClientDBLogic(QObject* parent = nullptr);

      void Init(const Chat::LoggerPtr& loggerPtr, const ApplicationSettingsPtr& appSettings);
   };
}

#endif // ClientDBLogic_h__