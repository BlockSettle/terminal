#ifndef ClientDBLogic_h__
#define ClientDBLogic_h__

#include <memory>

#include "ChatProtocol/DatabaseExecutor.h"
#include "ChatProtocol/ClientDatabaseCreator.h"

#include <google/protobuf/message.h>

class QSqlDatabase;
class ApplicationSettings;

namespace Chat
{
   using ApplicationSettingsPtr = std::shared_ptr<ApplicationSettings>;

   enum class ClientDBLogicError
   {
      InitDatabase
   };

   class ClientDBLogic : public DatabaseExecutor
   {
      Q_OBJECT

   public:
      ClientDBLogic(QObject* parent = nullptr);

      void Init(const Chat::LoggerPtr& loggerPtr, const Chat::ApplicationSettingsPtr& appSettings);

   signals:
      void initDone();
      void error(const Chat::ClientDBLogicError& errorCode, const std::string& what = "");

   private slots:
      void rebuildError();
      void handleLocalErrors(const Chat::ClientDBLogicError& errorCode, const std::string& what = "");
      void SaveMessage(const google::protobuf::Message& message);

   private:
      QSqlDatabase getDb() const;

      ApplicationSettingsPtr     applicationSettingsPtr_;
      ClientDatabaseCreatorPtr   databaseCreatorPtr_;
   };

   using ClientDBLogicPtr = std::shared_ptr<ClientDBLogic>;

}

Q_DECLARE_METATYPE(Chat::ClientDBLogicError);

#endif // ClientDBLogic_h__