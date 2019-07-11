#include "ChatDB.h"

#include <set>
#include <spdlog/spdlog.h>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QVariant>
#include <QDateTime>
#include "ChatProtocol/ChatUtils.h"
#include "ProtobufUtils.h"

ChatDB::ChatDB(const std::shared_ptr<spdlog::logger> &logger, const QString &dbFile)
   : logger_(logger)
   , requiredTables_({
      QLatin1String("user_keys"),
      QLatin1String("room_keys"),
      QLatin1String("messages"),
      QLatin1String("contacts")})
{
   db_ = QSqlDatabase::addDatabase(QLatin1String("QSQLITE"), QLatin1String("chat"));
   db_.setDatabaseName(dbFile);

   if (!db_.open()) {
      throw std::runtime_error("failed to open " + db_.connectionName().toStdString()
         + " DB: " + db_.lastError().text().toStdString());
   }

   createTable_ = {
      {QLatin1String("user_keys"), [db = db_] {
         const QLatin1String query("CREATE TABLE IF NOT EXISTS user_keys ("\
            "user_id CHAR(16) PRIMARY KEY,"\
            "key TEXT," \
            "key_timestamp DATETIME"\
            ");");
         if (!QSqlQuery(db).exec(query)) {
            return false;
         }
         return true;
      }},

      {QLatin1String("contacts"), [db = db_] {
         const QLatin1String query("CREATE TABLE IF NOT EXISTS contacts ("\
            "user_id CHAR(16) PRIMARY KEY,"\
            "user_name CHAR(64),"\
            "status INTEGER);");
           if (!QSqlQuery(db).exec(query)) {
               return false;
           }
           const QLatin1String qryIndexContactUserName("CREATE INDEX IF NOT EXISTS contact_by_user_name "\
              "ON contacts(user_name)");
           if (!QSqlQuery(db).exec(qryIndexContactUserName)) {
              return false;
           }
          const QLatin1String qryIndexContactUserId("CREATE INDEX IF NOT EXISTS contact_by_user_id "\
             "ON contacts(user_id)");
          if (!QSqlQuery(db).exec(qryIndexContactUserId)) {
             return false;
          }
           return true;
       }},

      {QLatin1String("room_keys"), [db = db_] {
         const QLatin1String query("CREATE TABLE IF NOT EXISTS room_keys ("\
            "room CHAR(32) PRIMARY KEY,"\
            "key TEXT"\
            ");");
         if (!QSqlQuery(db).exec(query)) {
            return false;
         }
         return true;
      }},

      {QLatin1String("messages"), [db = db_] {
         const QLatin1String query("CREATE TABLE IF NOT EXISTS messages ("\
            "id CHAR(16) NOT NULL,"\
            "timestamp INTEGER NOT NULL,"\
            "sender CHAR(16) NOT NULL,"\
            "receiver CHAR(32),"\
            "state INTEGER,"\
            "encryption INTEGER,"\
            "nonce BLOB,"\
            "enctext TEXT,"\
            "reference CHAR(16)"\
            ");");
         if (!QSqlQuery(db).exec(query)) {
            return false;
         }
         const QLatin1String qryIndexSender("CREATE INDEX messages_by_sender "\
            "ON messages(sender)");
         if (!QSqlQuery(db).exec(qryIndexSender)) {
            return false;
         }
         const QLatin1String qryIndexRecv("CREATE INDEX messages_by_receiver "\
            "ON messages(receiver)");
         if (!QSqlQuery(db).exec(qryIndexRecv)) {
            return false;
         }
         const QLatin1String qryIndexRef("CREATE INDEX messages_by_reference "\
            "ON messages(reference)");
         if (!QSqlQuery(db).exec(qryIndexRef)) {
            return false;
         }
         return true;
      }}
   };
   if (!createMissingTables()) {
      throw std::runtime_error("failed to create tables in " + db_.connectionName().toStdString() + " DB");
   }
}

bool ChatDB::createMissingTables()
{
   const auto &existingTables = db_.tables();
   std::set<QString> tableSet;
   tableSet.insert(existingTables.begin(), existingTables.end());

   bool result = true;
   for (const auto &reqTable : requiredTables_) {
      if (tableSet.find(reqTable) == tableSet.end()) {
         logger_->debug("[ChatDB] creating table {}", reqTable.toStdString());
         const bool rc = createTable_[reqTable]();
         if (!rc) {
            logger_->warn("[ChatDB] failed to create table {}", reqTable.toStdString());
         }
         result &= rc;
      }
   }
   return result;
}

bool ChatDB::isRoomMessagesExist(const std::string &roomId)
{
   // QSqlQuery qryAdd(QLatin1String("INSERT INTO messages(id, timestamp, sender, receiver, enctext, state, reference)"\
      // " VALUES(?, ?, ?, ?, ?, ?, ?);"), db_);

   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT receiver FROM messages WHERE receiver=:room_id;"))) {
      logger_->error("[ChatDB::isContactExist] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }
   query.bindValue(QStringLiteral(":room_id"), QString::fromStdString(roomId));
   if (!query.exec()) {
      logger_->error("[ChatDB::isContactExist] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   if (query.next()) {
      return true;
   }

   return false;
}

bool ChatDB::add(const std::shared_ptr<Chat::Data>& msg)
{
   const auto &d = msg->message();

   QSqlQuery qryAdd(db_);

   qryAdd.prepare(QLatin1String("INSERT INTO messages(id, timestamp, sender, receiver, state, encryption, nonce, enctext, reference)"\
                                " VALUES(:id, :tstamp, :sid, :rid, :state, :enctype, :nonce, :enctxt, :ref);"));
   qryAdd.bindValue(QLatin1String(":id"), QString::fromStdString(d.id()));
   qryAdd.bindValue(QLatin1String(":tstamp"), qint64(d.timestamp_ms()));
   qryAdd.bindValue(QLatin1String(":sid"), QString::fromStdString(d.sender_id()));
   qryAdd.bindValue(QLatin1String(":rid"), QString::fromStdString(d.receiver_id()));
   qryAdd.bindValue(QLatin1String(":state"), d.state());
   qryAdd.bindValue(QLatin1String(":enctype"), static_cast<int>(d.encryption()));
   qryAdd.bindValue(QLatin1String(":nonce"),
                    QByteArray(reinterpret_cast<const char*>(d.nonce().data()),
                               static_cast<int>(d.nonce().size()))
                    );
   qryAdd.bindValue(QLatin1String(":enctxt"), QString::fromStdString(d.message()));
   qryAdd.bindValue(QLatin1String(":ref"), QString());

//   qryAdd.bindValue(0, msg.getId());
//   qryAdd.bindValue(1, msg.getDateTime());
//   qryAdd.bindValue(2, msg.getSenderId());
//   qryAdd.bindValue(3, msg.getReceiverId());
//   qryAdd.bindValue(4, msg.getMessageData());
//   qryAdd.bindValue(5, msg.getState());
//   qryAdd.bindValue(6, QString());

   if (!qryAdd.exec()) {
      logger_->error("[ChatDB::add] failed to insert to changed: Error: {} Query:{}", qryAdd.lastError().text().toStdString(), qryAdd.lastQuery().toStdString());
      return false;
   }
   return true;
}

bool ChatDB::syncMessageId(const std::string& localId, const std::string& serverId)
{
   const QString cmd = QLatin1String("UPDATE messages SET id = :server_mid, state = state | :set_flags WHERE (id = :local_mid);");
   QSqlQuery query(db_);

   query.prepare(cmd);
   query.bindValue(QStringLiteral(":server_mid"), QString::fromStdString(serverId));
   query.bindValue(QStringLiteral(":local_mid"), QString::fromStdString(localId));
   query.bindValue(QStringLiteral(":set_flags"), static_cast<int>(Chat::Data_Message_State_SENT));

   if (!query.exec()) {
      logger_->error("[ChatDB::syncMessageId] failed to synchronize local message id with server message id; Error: {}",
                     query.lastError().text().toStdString()
                     );
      return false;
   }
   return true;
}

bool ChatDB::updateMessageStatus(const std::string& messageId, int status)
{
   const QString cmd = QStringLiteral("UPDATE messages SET"
                                     " state = :state"
                                     " WHERE (id = :mid);");

   QSqlQuery query(db_);

   query.prepare(cmd);
   query.bindValue(QStringLiteral(":mid"), QString::fromStdString(messageId));
   query.bindValue(QStringLiteral(":state"), status);

   if (!query.exec()) {
      logger_->error("[ChatDB::updateMessageStatus] failed to update message status with server message id: {}; Error: {}\nQuery: {}",
                     messageId,
                     query.lastError().text().toStdString(),
                     query.executedQuery().toStdString()
                     );
      return false;
   }
   return true;
}

std::vector<std::shared_ptr<Chat::Data>> ChatDB::getUserMessages(const std::string &ownUserId, const std::string &userId)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT sender, receiver, id, timestamp, enctext, state, nonce, encryption FROM messages "\
      "WHERE (sender=:user AND receiver=:owner) OR (receiver=:user AND sender=:owner)"))) {
      logger_->error("[ChatDB::getUserMessages] failed to prepare query: {}", query.lastError().text().toStdString());
      return {};
   }
   query.bindValue(QLatin1String(":user"), QString::fromStdString(userId));
   query.bindValue(QLatin1String(":owner"), QString::fromStdString(ownUserId));
   if (!query.exec()) {
      logger_->error("[ChatDB::getUserMessages] failed to exec query: {}", query.lastError().text().toStdString());
      return {};
   }

   std::vector<std::shared_ptr<Chat::Data>> records;
   while (query.next()) {
      std::string id = query.value(QStringLiteral("id")).toString().toStdString();
      std::string senderId = query.value(QStringLiteral("sender")).toString().toStdString();
      std::string receiverId = query.value(QStringLiteral("receiver")).toString().toStdString();
      qint64 timestamp = query.value(QStringLiteral("timestamp")).toLongLong();
      std::string messageData = query.value(QStringLiteral("enctext")).toString().toStdString();
      int state = query.value(QStringLiteral("state")).toInt();
      QByteArray nonce = query.value(QStringLiteral("nonce")).toByteArray();
      auto encryption = static_cast<Chat::Data_Message::Encryption>(query.value(QStringLiteral("encryption")).toInt());

      auto msg = std::make_shared<Chat::Data>();
      msg->mutable_message()->set_id(id);
      msg->mutable_message()->set_sender_id(senderId);
      msg->mutable_message()->set_receiver_id(receiverId);
      msg->mutable_message()->set_state(state);
      msg->mutable_message()->set_encryption(encryption);
      msg->mutable_message()->set_timestamp_ms(timestamp);
      msg->mutable_message()->set_message(messageData);
      msg->mutable_message()->set_nonce(std::string(nonce.begin(), nonce.end()));
      msg->mutable_message()->set_loaded_from_history(true);

      records.push_back(msg);
   }
   std::sort(records.begin(), records.end(), [](const std::shared_ptr<Chat::Data> &a
      , const std::shared_ptr<Chat::Data> &b) {
      return a->message().timestamp_ms() < b->message().timestamp_ms();
   });
   return records;
}

std::vector<std::shared_ptr<Chat::Data>> ChatDB::getRoomMessages(const std::string& roomId)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT sender, receiver, id, timestamp, enctext, state FROM messages "\
      "WHERE (receiver=:roomid);"))) {
      logger_->error("[ChatDB::getRoomMessages] failed to prepare query: {}", query.lastError().text().toStdString());
      return {};
   }
   query.bindValue(QStringLiteral(":roomid"), QString::fromStdString(roomId));
   if (!query.exec()) {
      logger_->error("[ChatDB::getRoomMessages] failed to exec query: {}", query.lastError().text().toStdString());
      return {};
   }

   std::vector<std::shared_ptr<Chat::Data>> records;
   while (query.next()) {
      // will create decrypted message with plain text
      const auto msg = std::make_shared<Chat::Data>();
      msg->mutable_message()->set_sender_id(query.value(0).toString().toStdString());
      msg->mutable_message()->set_receiver_id(query.value(1).toString().toStdString());
      msg->mutable_message()->set_id(query.value(2).toString().toStdString());
      msg->mutable_message()->set_timestamp_ms(query.value(3).toLongLong());
      msg->mutable_message()->set_message(query.value(4).toString().toStdString());
      msg->mutable_message()->set_state(query.value(5).toInt());
      msg->mutable_message()->set_loaded_from_history(true);
      records.push_back(msg);
   }
   std::sort(records.begin(), records.end(), [](const std::shared_ptr<Chat::Data> &a
      , const std::shared_ptr<Chat::Data> &b) {
      return a->message().timestamp_ms() < b->message().timestamp_ms();
   });
   return records;
}

bool ChatDB::removeRoomMessages(const std::string &roomId) {
   if (!isRoomMessagesExist(roomId)) {
      return false;
   }

   QSqlQuery query(QLatin1String(
      "DELETE FROM messages WHERE receiver=:room_id;"), db_);
   query.bindValue(0, QString::fromStdString(roomId));

   if (!query.exec()) {
      logger_->error("[ChatDB::removeContact] failed to delete contact.");
      return false;
   }

   return true;
}

std::map<std::string, BinaryData> ChatDB::loadKeys(bool* loaded)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT user_id, key FROM user_keys"))) {
      logger_->error("[ChatDB::loadKeys] failed to prepare query: {}", query.lastError().text().toStdString());
      if (loaded != nullptr) {
         *loaded = false;
      }
      return {};
   }
   if (!query.exec()) {
      logger_->error("[ChatDB::loadKeys] failed to exec query: {}", query.lastError().text().toStdString());
      if (loaded != nullptr) {
         *loaded = false;
      }
      return {};
   }

   std::map<std::string, BinaryData> keys_out;

   while (query.next()) {
      keys_out.emplace(query.value(0).toString().toStdString()
         , BinaryData::CreateFromHex(query.value(1).toString().toStdString()));
   }
   if (loaded != nullptr) {
      *loaded = true;
   }
   return keys_out;
}

bool ChatDB::addKey(const std::string& userId, const BinaryData& key, const QDateTime& dt)
{
   QSqlQuery qryAdd(QLatin1String(
      "INSERT INTO user_keys(user_id, key, key_timestamp) VALUES(?, ?, ?);"), db_);
   qryAdd.bindValue(0, QString::fromStdString(userId));
   qryAdd.bindValue(1, QString::fromStdString(key.toHexStr()));
   qryAdd.bindValue(2, dt);

   if (!qryAdd.exec()) {
      logger_->error("[ChatDB::addKey] failed to insert new public key value to user_keys.");
      return false;
   }
   return true;
}

bool ChatDB::removeKey(const std::string& userId)
{
   QSqlQuery query(db_);

   if (!query.prepare(QLatin1String("DELETE FROM user_keys WHERE user_id=:user_id;"))) {
      logger_->error("[ChatDB::{}] failed to prepare query: {}", __func__, query.lastError().text().toStdString());
      return false;
   }

   query.bindValue(QStringLiteral(":user_id"), QString::fromStdString(userId));

   if (!query.exec()) {
      logger_->error("[ChatDB::{}] failed to exec query: {}", __func__, query.lastError().text().toStdString());
      return false;
   }

   return true;
}

bool ChatDB::isContactExist(const std::string &userId)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT user_id FROM contacts WHERE user_id=:user_id;"))) {
      logger_->error("[ChatDB::isContactExist] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }
   query.bindValue(QLatin1String(":user_id"), QString::fromStdString(userId));

   if (!query.exec()) {
      logger_->error("[ChatDB::isContactExist] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   if (query.next()) {
      return true;
   }

   return false;
}

bool ChatDB::addContact(Chat::Data &contact)
{
   if (isContactExist(contact.contact_record().user_id())) {
      return true;
   }

   QSqlQuery query(db_);
   query.prepare(QLatin1String("INSERT INTO contacts(user_id, user_name, status) VALUES(:user_id, :user_name, :status)"));

   query.bindValue(QLatin1String(":user_id"), QString::fromStdString(contact.contact_record().user_id()));
   query.bindValue(QLatin1String(":user_name"), QString::fromStdString(contact.contact_record().display_name()));
   query.bindValue(QLatin1String(":status"), static_cast<int>(contact.contact_record().status()));

   if (!query.exec()) {
      logger_->error("[ChatDB::addContact] failed to insert new contact: {}", query.lastError().text().toStdString());
      return false;
   }

   return true;
}

bool ChatDB::removeContact(const std::string &userId)
{
   if (!isContactExist(userId)) {
      return true;
   }

   QSqlQuery query(QLatin1String(
      "DELETE FROM contacts WHERE user_id=?;"), db_);
   query.bindValue(0, QString::fromStdString(userId));

   if (!query.exec()) {
      logger_->error("[ChatDB::removeContact] failed to delete contact.");
      return false;
   }

   // remove also user public key
   removeKey(userId);

   return true;
}

bool ChatDB::getContacts(ContactRecordDataList &contactList)
{
   QSqlQuery query(db_);

   if (!query.prepare(QLatin1String(
      "SELECT contacts.user_id, contacts.user_name, contacts.status, user_keys.key, user_keys.key_timestamp FROM contacts " \
      "LEFT JOIN user_keys on contacts.user_id=user_keys.user_id;"))) {

      logger_->error("[ChatDB::getContacts] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }

   if (!query.exec()) {
      logger_->error("[ChatDB::getContacts] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   while (query.next()) {
      Chat::Data_ContactRecord contact;
      contact.set_user_id(query.value(0).toString().toStdString());
      contact.set_contact_id(query.value(0).toString().toStdString());
      contact.set_status(static_cast<Chat::ContactStatus>(query.value(2).toInt()));
      contact.set_public_key(BinaryData::CreateFromHex(query.value(3).toString().toStdString()).toBinStr());
      contact.set_public_key_timestamp(query.value(4).toDateTime().toMSecsSinceEpoch());

      auto name = query.value(1).toString().toStdString();
      if (!name.empty()) {
         contact.set_display_name(name);
      } else {
         contact.set_display_name(contact.contact_id());
      }

      contactList.emplace_back(contact);
   }
   return true;
}

bool ChatDB::updateContact(Chat::Data &contact)
{
   assert(contact.has_contact_record());

   QSqlQuery query(db_);

   if (!QString::fromStdString(contact.contact_record().display_name()).simplified().isEmpty()) {
      if (!query.prepare(QLatin1String("UPDATE contacts SET user_name=:user_name, status=:status WHERE user_id=:user_id;"))) {
         logger_->error("[ChatDB::updateContact] failed to prepare query: {}", query.lastError().text().toStdString());
         return false;
      }
      query.bindValue(QLatin1String(":user_name"), QString::fromStdString(contact.contact_record().display_name()));
      query.bindValue(QLatin1String(":status"), static_cast<int>(contact.contact_record().status()));
      query.bindValue(QLatin1String(":user_id"), QString::fromStdString(contact.contact_record().user_id()));
   } else {
      if (!query.prepare(QLatin1String("UPDATE contacts SET status=:status WHERE user_id=:user_id;"))) {
         logger_->error("[ChatDB::updateContact] failed to prepare query: {}", query.lastError().text().toStdString());
         return false;
      }
      query.bindValue(QLatin1String(":status"), static_cast<int>(contact.contact_record().status()));
      query.bindValue(QLatin1String(":user_id"), QString::fromStdString(contact.contact_record().user_id()));
   }

   if (!query.exec()) {
      logger_->error("[ChatDB::updateContact] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   return true;
}

bool ChatDB::getContact(const std::string &userId, Chat::Data_ContactRecord *contact)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String(
      "SELECT contacts.user_id, contacts.user_name, contacts.status, user_keys.key, user_keys.key_timestamp FROM contacts " \
      "LEFT JOIN user_keys on contacts.user_id=user_keys.user_id " \
      "WHERE contacts.user_id=?;"))) {
      logger_->error("[ChatDB::getContact] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }

   query.bindValue(0, QString::fromStdString(userId));

   if (!query.exec()) {
      logger_->error("[ChatDB::getContact] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   if (query.next()) {
      contact->set_user_id(query.value(0).toString().toStdString());
      contact->set_contact_id(query.value(0).toString().toStdString());
      contact->set_display_name(query.value(1).toString().toStdString());
      contact->set_status(static_cast<Chat::ContactStatus>(query.value(2).toInt()));
      contact->set_public_key(BinaryData::CreateFromHex(query.value(3).toString().toStdString()).toBinStr());
      contact->set_public_key_timestamp(query.value(4).toDateTime().toMSecsSinceEpoch());
      return true;
   }

   return false;
}

bool ChatDB::compareLocalData(const std::string& userId, const BinaryData& key, const QDateTime& dt)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT key, key_timestamp from user_keys WHERE user_id=:user_id;"))) {
      logger_->error("[ChatDB::{}] failed to prepare query: {}", __func__, query.lastError().text().toStdString());
      return false;
   }

   query.bindValue(QLatin1String(":user_id"), QString::fromStdString(userId));

   if (!query.exec()) {
      logger_->error("[ChatDB::{}] failed to exec query: {}", __func__, query.lastError().text().toStdString());
      return false;
   }

   if (query.next()) {
      BinaryData localKey = BinaryData::CreateFromHex(query.value(0).toString().toStdString());
      QDateTime localDt = query.value(1).toDateTime();

      if (localKey == key && localDt == dt) {
         return true;
      }

      return false;
   }

   logger_->error("[ChatDB::{}] contact: {} does not exist in db!", __func__, userId);
   return false;
}

bool ChatDB::updateContactKey(const std::string& userId, const BinaryData& publicKey, const QDateTime& publicKeyTimestamp)
{
   const QString cmd = QStringLiteral("UPDATE user_keys SET"
      " key = :key,"
      " key_timestamp = :key_timestamp"
      " WHERE (user_id = :user_id);");

   QSqlQuery query(db_);

   if (!query.prepare(cmd)) {
      logger_->error("[ChatDB::{}] failed to prepare query: {}", __func__, query.lastError().text().toStdString());
      return false;
   }

   query.bindValue(QStringLiteral(":key"), QString::fromStdString(publicKey.toHexStr()));
   query.bindValue(QStringLiteral(":key_timestamp"), publicKeyTimestamp);
   query.bindValue(QStringLiteral(":user_id"), QString::fromStdString(userId));

   if (!query.exec()) {
      logger_->error("[ChatDB::{}] failed to update user_keys; Error: {}\nQuery: {}",
         __func__,
         query.lastError().text().toStdString(),
         query.executedQuery().toStdString()
      );
      return false;
   }
   return true;
}
