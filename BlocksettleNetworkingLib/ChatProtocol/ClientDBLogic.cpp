#include <QThread>
#include <QSqlDatabase>
#include <QSqlError>

#include "ChatProtocol/ClientDBLogic.h"
#include "ApplicationSettings.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

#include "chat.pb.h"

namespace Chat
{

   ClientDBLogic::ClientDBLogic(QObject* parent /* = nullptr */) : DatabaseExecutor(parent)
   {
      qRegisterMetaType<Chat::ClientDBLogicError>();

      connect(this, &ClientDBLogic::error, this, &ClientDBLogic::handleLocalErrors);
   }

   void ClientDBLogic::Init(const Chat::LoggerPtr& loggerPtr, const Chat::ApplicationSettingsPtr& appSettings)
   {
      loggerPtr_ = loggerPtr;
      applicationSettingsPtr_ = appSettings;

      setLogger(loggerPtr);

      databaseCreatorPtr_ = std::make_shared<ClientDatabaseCreator>(getDb(), loggerPtr_, this);
      connect(databaseCreatorPtr_.get(), &ClientDatabaseCreator::rebuildDone, this, &ClientDBLogic::initDone);
      connect(databaseCreatorPtr_.get(), &ClientDatabaseCreator::rebuildError, this, &ClientDBLogic::rebuildError);

      databaseCreatorPtr_->rebuildDatabase();
   }

   QSqlDatabase ClientDBLogic::getDb() const
   {
      auto connectionName = QLatin1String("bs_chat_db_connection_") + QString::number(reinterpret_cast<quint64>(QThread::currentThread()), 16);

      if (!QSqlDatabase::contains(connectionName))
      {
         auto db = QSqlDatabase::addDatabase(QLatin1String("QSQLITE"), connectionName);
         //appSettings->get<QString>(ApplicationSettings::chatDbFile)
         db.setDatabaseName(applicationSettingsPtr_->get<QString>(ApplicationSettings::chatDbFile));

         if (!db.open()) {
            throw std::runtime_error("failed to open " + db.connectionName().toStdString()
               + " DB: " + db.lastError().text().toStdString());
         }

         return db;
      }

      return QSqlDatabase::database(connectionName);
   }

   void ClientDBLogic::rebuildError()
   {
      emit error(ClientDBLogicError::InitDatabase);
   }

   void ClientDBLogic::handleLocalErrors(const Chat::ClientDBLogicError& errorCode, const std::string& what /* = "" */)
   {
      loggerPtr_->debug("[ClientDBLogic::handleLocalErrors] Error {}, what: {}", static_cast<int>(errorCode), what);
   }

   void ClientDBLogic::SaveMessage(const google::protobuf::Message& message)
   {
      PartyMessagePacket partyMessagePacket;
      partyMessagePacket.CopyFrom(message);

   }

}