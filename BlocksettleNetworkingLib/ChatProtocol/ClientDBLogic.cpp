#include <QThread>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>
#include <QMetaObject>

#include "ChatProtocol/ClientDBLogic.h"
#include "ChatProtocol/CryptManager.h"
#include "ChatProtocol/Message.h"
#include "ApplicationSettings.h"
#include "ProtobufUtils.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include "BinaryData.h"
#include <enable_warnings.h>

#include "chat.pb.h"

using namespace Chat;

ClientDBLogic::ClientDBLogic(QObject* parent /* = nullptr */) : DatabaseExecutor(parent)
{
   qRegisterMetaType<Chat::ClientDBLogicError>();
   qRegisterMetaType<Chat::MessagePtr>();
   qRegisterMetaType<Chat::MessagePtrList>();

   connect(this, &ClientDBLogic::error, this, &ClientDBLogic::handleLocalErrors);
}

void ClientDBLogic::Init(const Chat::LoggerPtr& loggerPtr, const Chat::ApplicationSettingsPtr& appSettings, const ChatUserPtr& chatUserPtr,
   const Chat::CryptManagerPtr& cryptManagerPtr)
{
   loggerPtr_ = loggerPtr;
   applicationSettingsPtr_ = appSettings;
   currentChatUserPtr_ = chatUserPtr;
   cryptManagerPtr_ = cryptManagerPtr;

   setLogger(loggerPtr);

   databaseCreatorPtr_ = std::make_shared<ClientDatabaseCreator>(getDb(), loggerPtr_, this);
   connect(databaseCreatorPtr_.get(), &ClientDatabaseCreator::rebuildDone, this, &ClientDBLogic::initDone);
   connect(databaseCreatorPtr_.get(), &ClientDatabaseCreator::rebuildError, this, &ClientDBLogic::rebuildError);

   databaseCreatorPtr_->rebuildDatabase();
}

QSqlDatabase ClientDBLogic::getDb()
{
   auto connectionName = QLatin1String("bs_chat_db_connection_") + QString::number(reinterpret_cast<quint64>(QThread::currentThread()), 16);

   if (!QSqlDatabase::contains(connectionName))
   {
      auto db = QSqlDatabase::addDatabase(QLatin1String("QSQLITE"), connectionName);

      db.setDatabaseName(applicationSettingsPtr_->get<QString>(ApplicationSettings::chatDbFile));

      try
      {
         if (!db.open())
         {
            throw std::runtime_error("failed to open " + db.connectionName().toStdString()
               + " DB: " + db.lastError().text().toStdString());
         }
      }
      catch (const std::exception& e)
      {
         emit error(ClientDBLogicError::CannotOpenDatabase, e.what());
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

void ClientDBLogic::saveMessage(const std::string& data)
{
   PartyMessagePacket partyMessagePacket;
   if (!ProtobufUtils::pbStringToMessage<PartyMessagePacket>(data, &partyMessagePacket))
   {
      emit error(ClientDBLogicError::PartyMessagePacketCasting, data);
      return;
   }

   QFuture<std::string> future = cryptManagerPtr_->encryptMessageIES(partyMessagePacket.message(), currentChatUserPtr_->publicKey());
   std::string encryptedMessage = future.result();

   std::string tablePartyId;

   if (!getPartyIdFromDB(partyMessagePacket.party_id(), tablePartyId))
   {
      emit error(ClientDBLogicError::GetTablePartyId, partyMessagePacket.message_id());
      return;
   }

   const QString cmd = QLatin1String("INSERT INTO party_message (party_table_id, message_id, timestamp, message_state, encryption_type, nonce, message_text, sender) "
      "VALUES (:party_table_id, :message_id, :timestamp, :message_state, :encryption_type, :nonce, :message_text, :sender)");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QLatin1String(":party_table_id"), QString::fromStdString(tablePartyId));
   query.bindValue(QLatin1String(":message_id"), QString::fromStdString(partyMessagePacket.message_id()));
   query.bindValue(QLatin1String(":timestamp"), qint64(partyMessagePacket.timestamp_ms()));
   query.bindValue(QLatin1String(":message_state"), partyMessagePacket.party_message_state());
   query.bindValue(QLatin1String(":encryption_type"), partyMessagePacket.encryption());
   query.bindValue(QLatin1String(":nonce"), QByteArray::fromStdString(partyMessagePacket.nonce()));
   query.bindValue(QLatin1String(":message_text"), QString::fromStdString(encryptedMessage));
   query.bindValue(QLatin1String(":sender"), QString::fromStdString(partyMessagePacket.sender_hash()));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::SaveMessage, partyMessagePacket.message_id());
      return;
   }

   // ! signaled by ClientPartyModel in gui
   MessagePtr messagePtr = std::make_shared<Message>(partyMessagePacket.party_id(), partyMessagePacket.message_id(),
      QDateTime::fromMSecsSinceEpoch(partyMessagePacket.timestamp_ms()), partyMessagePacket.party_message_state(), partyMessagePacket.message(),
      partyMessagePacket.sender_hash());

   MessagePtrList messagePtrList;
   messagePtrList.push_back(messagePtr);

   emit messageArrived(messagePtrList);
}

void ClientDBLogic::createNewParty(const std::string& partyId)
{
   std::string idTableParty;
   if (getPartyIdFromDB(partyId, idTableParty))
   {
      // party already exist in db
      return;
   }
}

bool ClientDBLogic::insertPartyId(const std::string& partyId, std::string& partyTableId)
{
   const QString cmd = QLatin1String("INSERT INTO party (party_id, party_display_name) VALUES (:party_id, :party_display_name);");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QLatin1String(":party_id"), QString::fromStdString(partyId));
   // at beginning we using this same partyId as display name
   query.bindValue(QLatin1String(":party_display_name"), QString::fromStdString(partyId));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::InsertPartyId, partyId);
      return false;
   }

   auto id = query.lastInsertId().toULongLong();

   partyTableId = QStringLiteral("%1").arg(id).toStdString();
   return true;
}

bool ClientDBLogic::getPartyIdFromDB(const std::string& partyId, std::string& partyTableId)
{
   const QString cmd = QLatin1String("SELECT party.id FROM party WHERE party.party_id = :party_id;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QLatin1String(":party_id"), QString::fromStdString(partyId));

   if (checkExecute(query))
   {
      if (query.first())
      {
         int id = query.value(0).toULongLong();

         partyTableId = QStringLiteral("%1").arg(id).toStdString();
         return true;
      }
   }

   // if not exist create new one
   return insertPartyId(partyId, partyTableId);
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
      "LEFT JOIN party_message ON party_message.party_table_id = party.id "
      "WHERE party_message.message_id = :message_id;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QLatin1String(":message_id"), QString::fromStdString(messageId));

   if (checkExecute(query))
   {
      if (query.first())
      {
         partyId = query.value(0).toString().toStdString();
         return true;
      }
   }

   return false;
}

void ClientDBLogic::readUnsentMessages(const std::string& partyId)
{
   const QString cmd = QLatin1String("SELECT party_id, message_id, timestamp, message_state, encryption_type, nonce, message_text FROM party_message "
      "LEFT JOIN party on party.id = party_message.party_table_id "
      "WHERE party.party_id = :partyId AND message_state=:message_state;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QLatin1String(":partyId"), QString::fromStdString(partyId));
   query.bindValue(QLatin1String(":message_state"), static_cast<int>(PartyMessageState::UNSENT));

   if (!checkExecute(query))
   {
      return;
   }

   while (query.next())
   {
      PartyMessagePacket partyMessagePacket;
      partyMessagePacket.set_party_id(query.value(0).toString().toStdString());
      partyMessagePacket.set_message_id(query.value(1).toString().toStdString());
      partyMessagePacket.set_timestamp_ms(query.value(2).toLongLong());
      partyMessagePacket.set_party_message_state(static_cast<PartyMessageState>(query.value(3).toInt()));
      partyMessagePacket.set_encryption(static_cast<EncryptionType>(query.value(4).toInt()));
      partyMessagePacket.set_nonce(query.value(5).toString().toStdString());
      partyMessagePacket.set_message(query.value(6).toString().toStdString());

      QFuture<std::string> future = cryptManagerPtr_->decryptMessageIES(partyMessagePacket.message(), currentChatUserPtr_->privateKey());
      std::string decryptedMessage = future.result();

      emit messageLoaded(partyMessagePacket.party_id(), partyMessagePacket.message_id(), partyMessagePacket.timestamp_ms(),
         decryptedMessage, EncryptionType::UNENCRYPTED, partyMessagePacket.nonce(), partyMessagePacket.party_message_state());
   }
}

void ClientDBLogic::deleteMessage(const std::string& messageId)
{
   const QString cmd = QLatin1String("DELETE FROM party_message WHERE message_id=:message_id;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QLatin1String(":message_id"), QString::fromStdString(messageId));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::DeleteMessage, messageId);
      return;
   }

   return;
}

void ClientDBLogic::updateDisplayNameForParty(const std::string& partyId, const std::string& displayName)
{
   const QString cmd = QLatin1String("UPDATE party SET party_display_name=:displayName WHERE party_id=:partyId");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QLatin1String(":displayName"), QString::fromStdString(displayName));
   query.bindValue(QLatin1String(":partyId"), QString::fromStdString(partyId));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::UpdatePartyDisplayName, partyId);
      return;
   }

   return;
}

void ClientDBLogic::loadPartyDisplayName(const std::string& partyId)
{
   const QString cmd = QLatin1String("SELECT party_display_name FROM party WHERE party_id = :partyId;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QLatin1String(":partyId"), QString::fromStdString(partyId));

   if (checkExecute(query))
   {
      if (query.first())
      {
         std::string displayName = query.value(0).toString().toStdString();

         if (displayName == partyId)
         {
            return;
         }

         emit partyDisplayNameLoaded(partyId, displayName);
      }
   }
}

void ClientDBLogic::checkUnsentMessages(const std::string& partyId)
{
   const QString cmd = QLatin1String("SELECT message_state FROM party_message WHERE party_table_id=(SELECT id FROM party WHERE party_id=:partyId) AND message_state=0;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QLatin1String(":partyId"), QString::fromStdString(partyId));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::CheckUnsentMessages, partyId);
      return;
   }

   if (query.first())
   {
      emit unsentMessagesFound(partyId);
   }
}

void ClientDBLogic::readHistoryMessages(const std::string& partyId, const int limit, const int offset)
{
   const QString cmd =
      QStringLiteral(
         "SELECT message_id, timestamp, message_state, message_text, sender FROM party_message "
         "LEFT JOIN party ON party.id=party_message.party_table_id "
         "WHERE party_id=:partyId ORDER BY timestamp DESC LIMIT %1 OFFSET %2;"
      ).arg(limit).arg(offset);

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QLatin1String(":partyId"), QString::fromStdString(partyId));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::ReadHistoryMessages, partyId);
      return;
   }

   MessagePtrList messagePtrList;

   while (query.next())
   {
      std::string messageId = query.value(0).toString().toStdString();
      qint64 timestamp = query.value(1).toULongLong();
      PartyMessageState partyMessageState = static_cast<PartyMessageState>(query.value(2).toInt());
      std::string message = query.value(3).toString().toStdString();
      std::string senderId = query.value(4).toString().toStdString();

      QFuture<std::string> future = cryptManagerPtr_->decryptMessageIES(message, currentChatUserPtr_->privateKey());
      std::string decryptedMessage = future.result();

      MessagePtr messagePtr = std::make_shared<Message>(partyId, messageId, QDateTime::fromMSecsSinceEpoch(timestamp), partyMessageState, decryptedMessage, senderId);

      messagePtrList.push_back(messagePtr);
   }

   if (messagePtrList.empty())
   {
      return;
   }

   emit messageArrived(messagePtrList);
}
