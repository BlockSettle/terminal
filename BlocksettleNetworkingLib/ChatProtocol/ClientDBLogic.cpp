#include <QThread>
#include <QSqlError>
#include <QSqlQuery>
#include <QMetaObject>

#include "ChatProtocol/ClientDBLogic.h"
#include "ChatProtocol/CryptManager.h"
#include "ChatProtocol/Message.h"
#include "ChatProtocol/ClientParty.h"
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
   const auto connectionName = QStringLiteral("bs_chat_db_connection_") + QString::number(reinterpret_cast<quint64>(QThread::currentThread()), 16);

   if (!QSqlDatabase::contains(connectionName))
   {
      auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);

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

void ClientDBLogic::handleLocalErrors(const Chat::ClientDBLogicError& errorCode, const std::string& what /* = "" */) const
{
   loggerPtr_->debug("[ClientDBLogic::handleLocalErrors] Error {}, what: {}", static_cast<int>(errorCode), what);
}

void ClientDBLogic::saveMessage(const Chat::PartyPtr& partyPtr, const std::string& data)
{
   PartyMessagePacket partyMessagePacket;
   if (!ProtobufUtils::pbStringToMessage<PartyMessagePacket>(data, &partyMessagePacket))
   {
      emit error(ClientDBLogicError::PartyMessagePacketCasting, data);
      return;
   }

   const auto future = cryptManagerPtr_->encryptMessageIES(partyMessagePacket.message(), currentChatUserPtr_->publicKey());
   const auto encryptedMessage = future.result();

   std::string partyTableId;

   if (!getPartyTableIdFromDB(partyPtr, partyTableId))
   {
      emit error(ClientDBLogicError::GetTablePartyId, partyMessagePacket.message_id());
      return;
   }

   const auto cmd = QStringLiteral("INSERT INTO party_message (party_table_id, message_id, timestamp, message_state, encryption_type, nonce, message_text, sender) "
      "VALUES (:party_table_id, :message_id, :timestamp, :message_state, :encryption_type, :nonce, :message_text, :sender) "
      "ON CONFLICT(message_id) DO UPDATE SET "
      "party_table_id=:party_table_id, message_id=:message_id, timestamp=:timestamp, message_state=:message_state, "
      "encryption_type=:encryption_type, nonce=:nonce, message_text=:message_text, sender=:sender;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":party_table_id"), QString::fromStdString(partyTableId));
   query.bindValue(QStringLiteral(":message_id"), QString::fromStdString(partyMessagePacket.message_id()));
   query.bindValue(QStringLiteral(":timestamp"), qint64(partyMessagePacket.timestamp_ms()));
   query.bindValue(QStringLiteral(":message_state"), partyMessagePacket.party_message_state());
   query.bindValue(QStringLiteral(":encryption_type"), partyMessagePacket.encryption());
   query.bindValue(QStringLiteral(":nonce"), QByteArray::fromStdString(partyMessagePacket.nonce()));
   query.bindValue(QStringLiteral(":message_text"), QString::fromStdString(encryptedMessage));
   query.bindValue(QStringLiteral(":sender"), QString::fromStdString(partyMessagePacket.sender_hash()));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::SaveMessage, partyMessagePacket.message_id());
      return;
   }

   // message saved, update party to user table
   if (partyPtr->isPrivate())
   {
      // For private parties save all recipients belongs to this party
      savePartyRecipients(partyPtr);
   }

   // ! signaled by ClientPartyModel in gui
   const auto messagePtr = std::make_shared<Message>(partyMessagePacket.party_id(), partyMessagePacket.message_id(),
      QDateTime::fromMSecsSinceEpoch(partyMessagePacket.timestamp_ms()), partyMessagePacket.party_message_state(), partyMessagePacket.message(),
      partyMessagePacket.sender_hash());

   MessagePtrList messagePtrList;
   messagePtrList.push_back(messagePtr);

   emit messageArrived(messagePtrList);
}

void ClientDBLogic::createNewParty(const Chat::PartyPtr& partyPtr)
{
   std::string idTableParty;
   if (getPartyTableIdFromDB(partyPtr, idTableParty))
   {
      // party already exist in db
      return;
   }

   // if not exist create new one
   insertPartyId(partyPtr, idTableParty);
}

void ClientDBLogic::savePartyRecipients(const Chat::PartyPtr& partyPtr)
{
   const auto clientPartyPtr = std::dynamic_pointer_cast<ClientParty>(partyPtr);

   if (nullptr == clientPartyPtr)
   {
      return;
   }

   std::string partyTableId;
   if (!getPartyTableIdFromDB(partyPtr, partyTableId))
   {
      emit error(ClientDBLogicError::GetTablePartyId, partyPtr->id());
      return;
   }

   const auto cmd = QStringLiteral("INSERT INTO party_to_user (party_table_id, user_table_id) "
      "SELECT * FROM "
      "(SELECT :party_table_id, :user_table_id) AS tmp "
      "WHERE NOT EXISTS ( SELECT id FROM party_to_user WHERE party_table_id=:party_table_id AND user_table_id=:user_table_id ) LIMIT 1;");

   auto recipients = clientPartyPtr->recipients();

   for (const auto& recipient : recipients)
   {
      std::string userTableId;
      if (!getUserTableId(recipient->userHash(), userTableId))
      {
         // user not exist in db, save
         insertNewUserHash(recipient->userHash());

         getUserTableId(recipient->userHash(), userTableId);
      }

      QSqlQuery query(getDb());
      query.prepare(cmd);
      query.bindValue(QStringLiteral(":party_table_id"), QString::fromStdString(partyTableId));
      query.bindValue(QStringLiteral(":user_table_id"), QString::fromStdString(userTableId));

      if (!checkExecute(query))
      {
         emit error(ClientDBLogicError::InsertPartyToUser, partyPtr->id());
      }
   }
}

bool ClientDBLogic::getUserTableId(const std::string& userHash, std::string& userTableId)
{
   const auto cmd = QStringLiteral("SELECT user_id FROM user WHERE user_hash = :user_hash;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":user_hash"), QString::fromStdString(userHash));

   if (checkExecute(query))
   {
      if (query.first())
      {
         const int id = query.value(0).toULongLong();

         userTableId = QStringLiteral("%1").arg(id).toStdString();
         return true;
      }
   }

   return false;
}

bool ClientDBLogic::insertPartyId(const Chat::PartyPtr& partyPtr, std::string& partyTableId)
{
   const auto cmd = QStringLiteral("INSERT INTO party (party_id, party_display_name, party_type, party_sub_type) "
      "VALUES (:party_id, :party_display_name, :party_type, :party_sub_type) ON CONFLICT(party_id) DO UPDATE SET "
      "party_id=:party_id, party_display_name=:party_display_name, party_type=:party_type, party_sub_type=:party_sub_type;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":party_id"), QString::fromStdString(partyPtr->id()));
   // at beginning we using this same partyId as display name
   query.bindValue(QStringLiteral(":party_display_name"), QString::fromStdString(partyPtr->id()));
   query.bindValue(QStringLiteral(":party_type"), QStringLiteral("%1").arg(partyPtr->partyType()));
   query.bindValue(QStringLiteral(":party_sub_type"), QStringLiteral("%1").arg(partyPtr->partySubType()));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::InsertPartyId, partyPtr->id());
      return false;
   }

   const auto id = query.lastInsertId().toULongLong();

   partyTableId = QStringLiteral("%1").arg(id).toStdString();
   return true;
}

bool ClientDBLogic::getPartyTableIdFromDB(const Chat::PartyPtr& partyPtr, std::string& partyTableId)
{
   const auto cmd = QStringLiteral("SELECT party.id FROM party WHERE party.party_id = :party_id;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":party_id"), QString::fromStdString(partyPtr->id()));

   if (checkExecute(query))
   {
      if (query.first())
      {
         const int id = query.value(0).toULongLong();

         partyTableId = QStringLiteral("%1").arg(id).toStdString();
         return true;
      }
   }

   return false;
}

void ClientDBLogic::updateMessageState(const std::string& message_id, const int party_message_state)
{
   const auto cmd = QStringLiteral("UPDATE party_message SET message_state = :message_state WHERE message_id = :message_id;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":message_state"), party_message_state);
   query.bindValue(QStringLiteral(":message_id"), QString::fromStdString(message_id));

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
   const auto cmd = QStringLiteral("SELECT party_id FROM party "
      "LEFT JOIN party_message ON party_message.party_table_id = party.id "
      "WHERE party_message.message_id = :message_id;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":message_id"), QString::fromStdString(messageId));

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
   const auto cmd = QStringLiteral("SELECT party_id, message_id, timestamp, message_state, encryption_type, nonce, message_text FROM party_message "
      "LEFT JOIN party on party.id = party_message.party_table_id "
      "WHERE party.party_id = :partyId AND message_state=:message_state;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":partyId"), QString::fromStdString(partyId));
   query.bindValue(QStringLiteral(":message_state"), static_cast<int>(UNSENT));

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

      auto future = cryptManagerPtr_->decryptMessageIES(partyMessagePacket.message(), currentChatUserPtr_->privateKey());
      auto decryptedMessage = future.result();

      emit messageLoaded(partyMessagePacket.party_id(), partyMessagePacket.message_id(), partyMessagePacket.timestamp_ms(),
         decryptedMessage, UNENCRYPTED, partyMessagePacket.nonce(), partyMessagePacket.party_message_state());
   }
}

void ClientDBLogic::deleteMessage(const std::string& messageId)
{
   const auto cmd = QStringLiteral("DELETE FROM party_message WHERE message_id=:message_id;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":message_id"), QString::fromStdString(messageId));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::DeleteMessage, messageId);
   }
}

void ClientDBLogic::updateDisplayNameForParty(const std::string& partyId, const std::string& displayName)
{
   const auto cmd = QStringLiteral("UPDATE party SET party_display_name=:displayName WHERE party_id=:partyId");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":displayName"), QString::fromStdString(displayName));
   query.bindValue(QStringLiteral(":partyId"), QString::fromStdString(partyId));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::UpdatePartyDisplayName, partyId);
   }
}

void ClientDBLogic::loadPartyDisplayName(const std::string& partyId)
{
   const auto cmd = QStringLiteral("SELECT party_display_name FROM party WHERE party_id = :partyId;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":partyId"), QString::fromStdString(partyId));

   if (checkExecute(query))
   {
      if (query.first())
      {
         const auto displayName = query.value(0).toString().toStdString();

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
   const auto cmd = QStringLiteral("SELECT message_state FROM party_message WHERE party_table_id=(SELECT id FROM party WHERE party_id=:partyId) AND message_state=0;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":partyId"), QString::fromStdString(partyId));

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

void ClientDBLogic::readPrivateHistoryMessages(const std::string& partyId, const std::string& userHash, const int limit, const int offset)
{
   // read all history messages for given userHash except OTC and Global parties
   const auto cmd =
      QStringLiteral(
         "SELECT message_id, timestamp, message_state, message_text, sender FROM party_message "
         "LEFT JOIN party on party.id=party_message.party_table_id "
         "LEFT JOIN party_to_user ON party_to_user.party_table_id = party_message.party_table_id "
         "LEFT JOIN user ON user.user_id = party_to_user.user_table_id "
         "WHERE user.user_hash = :user_hash "
         "AND party.party_sub_type <> 1 " // NOT OTC
         "AND party.party_type <> 0 " // NOT Global
         "ORDER BY timestamp DESC LIMIT %1 OFFSET %2; "
      ).arg(limit).arg(offset);

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":user_hash"), QString::fromStdString(userHash));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::ReadPrivateHistoryMessages, userHash);
      return;
   }

   MessagePtrList messagePtrList;

   while (query.next())
   {
      auto messageId = query.value(0).toString().toStdString();
      const qint64 timestamp = query.value(1).toULongLong();
      auto partyMessageState = static_cast<PartyMessageState>(query.value(2).toInt());
      auto message = query.value(3).toString().toStdString();
      auto senderId = query.value(4).toString().toStdString();

      auto future = cryptManagerPtr_->decryptMessageIES(message, currentChatUserPtr_->privateKey());
      auto decryptedMessage = future.result();

      auto messagePtr = std::make_shared<Message>(partyId, messageId, QDateTime::fromMSecsSinceEpoch(timestamp), partyMessageState, decryptedMessage, senderId);

      messagePtrList.push_back(messagePtr);
   }

   if (messagePtrList.empty())
   {
      return;
   }

   emit messageArrived(messagePtrList);
}

void ClientDBLogic::saveRecipientsKeys(const Chat::PartyRecipientsPtrList& recipients)
{
   for (const auto& recipient : recipients)
   {
      saveRecipientKey(recipient);
   }
}

void ClientDBLogic::insertNewUserHash(const std::string& userHash)
{
   // create new user
   const auto cmd = QStringLiteral("INSERT INTO user (user_hash) VALUES (:user_hash);");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":user_hash"), QString::fromStdString(userHash));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::InsertUser, userHash);
   }
}

void ClientDBLogic::saveRecipientKey(const Chat::PartyRecipientPtr& recipient)
{
   std::string userTableId;
   if (!getUserTableId(recipient->userHash(), userTableId))
   {
      insertNewUserHash(recipient->userHash());

      getUserTableId(recipient->userHash(), userTableId);
   }

   const auto cmd = QStringLiteral("INSERT INTO user_key (user_table_id, public_key, public_key_timestamp) "
      "VALUES (:user_table_id, :public_key, :public_key_timestamp) ON CONFLICT(user_table_id) DO UPDATE SET "
      "user_table_id=:user_table_id, public_key=:public_key, public_key_timestamp=:public_key_timestamp;");

   QSqlQuery query(getDb());
   query.prepare(cmd);
   query.bindValue(QStringLiteral(":user_table_id"), QString::fromStdString(userTableId));
   query.bindValue(QStringLiteral(":public_key"), QString::fromStdString(recipient->publicKey().toHexStr()));
   query.bindValue(QStringLiteral(":public_key_timestamp"), qint64(recipient->publicKeyTime().toMSecsSinceEpoch()));

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::InsertRecipientKey, recipient->userHash());
   }
}

void ClientDBLogic::deleteRecipientsKeys(const Chat::PartyRecipientsPtrList& recipients)
{
   const auto cmd = QStringLiteral("DELETE FROM user_key WHERE user_table_id = (SELECT user_id from user WHERE user_hash=:user_hash);");

   for (const auto& recipient : recipients)
   {
      QSqlQuery query(getDb());
      query.prepare(cmd);
      query.bindValue(QStringLiteral(":user_hash"), QString::fromStdString(recipient->userHash()));

      if (!checkExecute(query))
      {
         emit error(ClientDBLogicError::DeleteRecipientKey, recipient->userHash());
      }
   }
}

void ClientDBLogic::updateRecipientKeys(const Chat::PartyRecipientsPtrList& recipients)
{
   for (const auto& recipient : recipients)
   {
      saveRecipientKey(recipient);
   }
}

void ClientDBLogic::checkRecipientPublicKey(const Chat::UniqieRecipientMap& uniqueRecipientMap)
{
   const auto cmd = QStringLiteral("SELECT public_key, public_key_timestamp FROM user_key "
      "WHERE user_table_id = (SELECT user_id FROM user WHERE user_hash=:user_hash);");
   UserPublicKeyInfoList userPkList;

   for (const auto& uniqueRecipient : uniqueRecipientMap)
   {
      const auto recipientPtr = uniqueRecipient.second;

      QSqlQuery query(getDb());
      query.prepare(cmd);
      query.bindValue(QStringLiteral(":user_hash"), QString::fromStdString(recipientPtr->userHash()));

      if (checkExecute(query))
      {
         if (query.first())
         {
            auto oldPublicKey = BinaryData::CreateFromHex(query.value(0).toString().toStdString());
            auto oldPublicKeyTimestamp = QDateTime::fromMSecsSinceEpoch(query.value(1).toULongLong());

            if (recipientPtr->publicKey() != oldPublicKey || recipientPtr->publicKeyTime() != oldPublicKeyTimestamp)
            {
               const auto userPkPtr = std::make_shared<UserPublicKeyInfo>();
               userPkPtr->setUser_hash(QString::fromStdString(recipientPtr->userHash()));
               userPkPtr->setOldPublicKeyHex(oldPublicKey);
               userPkPtr->setOldPublicKeyTime(oldPublicKeyTimestamp);
               userPkPtr->setNewPublicKeyHex(recipientPtr->publicKey());
               userPkPtr->setNewPublicKeyTime(recipientPtr->publicKeyTime());
               userPkList.push_back(userPkPtr);
            }
         }
         else
         {
            // key not exist, we assume that key was changed
            const auto userPkPtr = std::make_shared<UserPublicKeyInfo>();
            userPkPtr->setUser_hash(QString::fromStdString(recipientPtr->userHash()));
            userPkPtr->setNewPublicKeyHex(recipientPtr->publicKey());
            userPkPtr->setNewPublicKeyTime(recipientPtr->publicKeyTime());
            userPkList.push_back(userPkPtr);
         }
      }
      else
      {
         emit error(ClientDBLogicError::CheckRecipientKey, recipientPtr->userHash());
      }
   }

   if (userPkList.empty())
   {
      emit recipientKeysUnchanged();
      return;
   }

   emit recipientKeysHasChanged(userPkList);
}

void ClientDBLogic::clearUnusedParties()
{
   // keep only parties which have history
   const auto cmd = QStringLiteral(
      "DELETE FROM party WHERE id IN "
      "(SELECT id FROM party WHERE id NOT IN "
      "(SELECT party_table_id FROM party_message));");

   QSqlQuery query(getDb());
   query.prepare(cmd);

   if (!checkExecute(query))
   {
      emit error(ClientDBLogicError::CleanUnusedParties);
      return;
   }

   // clean party_to_user
   const auto cmdPtu = QStringLiteral(
      "DELETE FROM party_to_user WHERE party_table_id NOT IN "
      "(SELECT id FROM party);"
   );

   QSqlQuery queryPtu(getDb());
   queryPtu.prepare(cmdPtu);

   if (!checkExecute(queryPtu))
   {
      emit error(ClientDBLogicError::CleanUnusedParties);
   }
}
