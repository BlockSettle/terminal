#include <QThread>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

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

   void ClientDBLogic::saveMessage(const google::protobuf::Message& message)
   {
      PartyMessagePacket partyMessagePacket;
      partyMessagePacket.CopyFrom(message);

      std::string tablePartyId;

      if (!getPartyIdFromDB(partyMessagePacket.party_id(), tablePartyId))
      {
         emit error(ClientDBLogicError::GetTablePartyId, partyMessagePacket.message_id());
         return;
      }

      const QString cmd = QLatin1String("INSERT INTO party_message (party_table_id, message_id, timestamp, message_state, encryption_type, nonce, message_text) "
         "VALUES (:party_table_id, :message_id, :timestamp, :message_state, :encryption_type, :nonce, :message_text)");

      QSqlQuery query(getDb());
      query.prepare(cmd);
      query.bindValue(QLatin1String(":party_table_id"), QString::fromStdString(tablePartyId));
      query.bindValue(QLatin1String(":message_id"), QString::fromStdString(partyMessagePacket.message_id()));
      query.bindValue(QLatin1String(":timestamp"), partyMessagePacket.timestamp_ms());
      query.bindValue(QLatin1String(":message_state"), partyMessagePacket.party_message_state());
      query.bindValue(QLatin1String(":encryption_type"), partyMessagePacket.encryption());
      query.bindValue(QLatin1String(":nonce"), QByteArray::fromStdString(partyMessagePacket.nonce()));
      query.bindValue(QLatin1String(":message_text"), QString::fromStdString(partyMessagePacket.message()));

      if (!checkExecute(query))
      {
         emit error(ClientDBLogicError::SaveMessage, partyMessagePacket.message_id());
         return;
      }

      // ! signaled by ClientPartyModel in gui
      emit messageInserted(partyMessagePacket.party_id(), partyMessagePacket.message_id(), partyMessagePacket.message(),
         partyMessagePacket.timestamp_ms(), partyMessagePacket.party_message_state());
   }

   bool ClientDBLogic::insertPartyId(const std::string& partyId, std::string& tablePartyId)
   {
      const QString cmd = QLatin1String("INSERT INTO party (partyId) VALUES (:party_id);");

      QSqlQuery query(getDb());
      query.prepare(cmd);
      query.bindValue(QLatin1String(":party_id"), QString::fromStdString(partyId));

      if (!checkExecute(query))
      {
         emit error(ClientDBLogicError::InsertPartyId, partyId);
         return false;
      }

      tablePartyId = query.lastInsertId().toString().toStdString();
      return true;
   }

   bool ClientDBLogic::getPartyIdFromDB(const std::string& partyId, std::string& tablePartyId)
   {
      const QString cmd = QLatin1String("SELECT id FROM party WHERE party_id = :party_id;");

      QSqlQuery query(getDb());
      query.prepare(cmd);
      query.bindValue(QLatin1String(":party_id"), QString::fromStdString(partyId));

      if (checkExecute(query))
      {
         if (query.first())
         {
            tablePartyId = query.value(QLatin1String("party_id")).toString().toStdString();
            return true;
         }
      }

      // if not exist create new one
      return insertPartyId(partyId, tablePartyId);
   }

   void ClientDBLogic::updateMessageState(const std::string& message_id, const int party_message_state)
   {
      const QString cmd = QLatin1String("UPDATE party_message SET message_state = :message_state WHERE message_id = :message_id;");

      QSqlQuery query(getDb());
      query.prepare(cmd);
      query.bindValue(QLatin1String(":message_state"), party_message_state);
      query.bindValue(QLatin1String(":message_id"), QString::fromStdString(message_id));

      if (!checkExecute(query))
      {
         emit error(ClientDBLogicError::UpdateMessageState, message_id);
         return;
      }

      std::string partyId;
      if (!getPartyIdByMessageId(message_id, partyId))
      {
         return;
      }

      emit messageStateChanged(partyId, message_id, party_message_state);
   }

   bool ClientDBLogic::getPartyIdByMessageId(const std::string& messageId, std::string& partyId)
   {
      const QString cmd = QLatin1String("SELECT party_id FROM party "
         "LEFT JOIN party_message ON party.party_id = party_message.party_table_id "
         "WHERE party_message.message_id = :message_id;");

      QSqlQuery query(getDb());
      query.bindValue(QLatin1String(":message_id"), QString::fromStdString(messageId));

      if (!checkExecute(query))
      {
         emit error(ClientDBLogicError::GetPartyIdByMessageId, messageId);
         return false;
      }

      if (query.first())
      {
         partyId = query.value(QLatin1String("party_id")).toString().toStdString();
         return true;
      }

      emit error(ClientDBLogicError::GetPartyIdByMessageIdNotFound, messageId);
      return false;
   }

}