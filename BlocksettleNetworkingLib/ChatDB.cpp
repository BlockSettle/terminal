#include "ChatDB.h"
#include <set>
#include <spdlog/spdlog.h>
#include <QDateTime>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QVariant>


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
            "user_name CHAR(64),"\
            "key TEXT"\
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

bool ChatDB::isRoomMessagesExist(const QString &roomId)
{
   // QSqlQuery qryAdd(QLatin1String("INSERT INTO messages(id, timestamp, sender, receiver, enctext, state, reference)"\
      // " VALUES(?, ?, ?, ?, ?, ?, ?);"), db_);

   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT receiver FROM messages WHERE receiver=:room_id;"))) {
      logger_->error("[ChatDB::isContactExist] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }
   query.bindValue(QString::fromStdString(":room_id"), roomId);
   if (!query.exec()) {
      logger_->error("[ChatDB::isContactExist] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   if (query.next()) {
      return true;
   }

   return false;
}

bool ChatDB::add(const Chat::MessageData &msg)
{
   QSqlQuery qryAdd(db_);

   qryAdd.prepare(QLatin1String("INSERT INTO messages(id, timestamp, sender, receiver, state, encryption, nonce, enctext, reference)"\
                                " VALUES(:id, :tstamp, :sid, :rid, :state, :enctype, :nonce, :enctxt, :ref);"));
   qryAdd.bindValue(QLatin1String(":id"), msg.id());
   qryAdd.bindValue(QLatin1String(":tstamp"), msg.dateTime());
   qryAdd.bindValue(QLatin1String(":sid"), msg.senderId());
   qryAdd.bindValue(QLatin1String(":rid"), msg.receiverId());
   qryAdd.bindValue(QLatin1String(":state"), msg.state());
   qryAdd.bindValue(QLatin1String(":enctype"), static_cast<int>(msg.encryptionType()));
   qryAdd.bindValue(QLatin1String(":nonce"),
                    QByteArray(reinterpret_cast<char*>(msg.nonce().data()),
                               static_cast<int>(msg.nonce().size()))
                    );
   qryAdd.bindValue(QLatin1String(":enctxt"), msg.messageData());
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

bool ChatDB::syncMessageId(const QString& localId, const QString& serverId)
{
   const QString cmd = QLatin1String("UPDATE messages SET id = :server_mid, state = state | :set_flags WHERE (id = :local_mid);");
   QSqlQuery query(db_);

   query.prepare(cmd);
   query.bindValue(QLatin1String(":server_mid"), serverId);
   query.bindValue(QLatin1String(":local_mid"), localId);
   query.bindValue(QLatin1String(":set_flags"), static_cast<int>(Chat::MessageData::State::Sent));
   
   if (!query.exec()) {
      logger_->error("[ChatDB::syncMessageId] failed to synchronize local message id with server message id; Error: {}",
                     query.lastError().text().toStdString()
                     );
      return false;
   }
   return true;
}

bool ChatDB::updateMessageStatus(const QString& messageId, int ustatus)
{
   const QString cmd = QLatin1String("UPDATE messages SET"
                                     " state = :state"
                                     " WHERE (id = :mid);");

   QSqlQuery query(db_);

   query.prepare(cmd);
   query.bindValue(QLatin1String(":mid"), messageId);
   query.bindValue(QLatin1String(":state"), ustatus);
   
   if (!query.exec()) {
      logger_->error("[ChatDB::updateMessageStatus] failed to update message status with server message id: {}; Error: {}\nQuery: {}",
                     messageId.toStdString(),
                     query.lastError().text().toStdString(),
                     query.executedQuery().toStdString()
                     );
      return false;
   }
   return true;
}

std::vector<std::shared_ptr<Chat::MessageData>> ChatDB::getUserMessages(const QString &ownUserId, const QString &userId)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT sender, receiver, id, timestamp, enctext, state, nonce, encryption FROM messages "\
      "WHERE (sender=:user AND receiver=:owner) OR (receiver=:user AND sender=:owner)"))) {
      logger_->error("[ChatDB::getUserMessages] failed to prepare query: {}", query.lastError().text().toStdString());
      return {};
   }
   query.bindValue(QString::fromStdString(":user"), userId);
   query.bindValue(QString::fromStdString(":owner"), ownUserId);
   if (!query.exec()) {
      logger_->error("[ChatDB::getUserMessages] failed to exec query: {}", query.lastError().text().toStdString());
      return {};
   }

   std::vector<std::shared_ptr<Chat::MessageData>> records;
   while (query.next()) {
      QString id = query.value(QLatin1String("id")).toString();
      QString senderId = query.value(QLatin1String("sender")).toString();
      QString receiverId = query.value(QLatin1String("receiver")).toString();
      QDateTime timestamp = query.value(QLatin1String("timestamp")).toDateTime();
      QString messageData = query.value(QLatin1String("enctext")).toString();
      int state = query.value(QLatin1String("state")).toInt();
      QByteArray nonce = query.value(QLatin1String("nonce")).toByteArray();
      Chat::MessageData::EncryptionType encryption = static_cast<Chat::MessageData::EncryptionType>(query.value(QLatin1String("encryption")).toInt());
      const auto msg = std::make_shared<Chat::MessageData>(senderId,
                                                           receiverId,
                                                           id,
                                                           timestamp,
                                                           messageData,
                                                           state);
      msg->setNonce(Botan::SecureVector<uint8_t>(nonce.begin(), nonce.end()));
      msg->setEncryptionType(encryption);
      records.push_back(msg);
   }
   std::sort(records.begin(), records.end(), [](const std::shared_ptr<Chat::MessageData> &a
      , const std::shared_ptr<Chat::MessageData> &b) {
      return (a->dateTime().toMSecsSinceEpoch() < b->dateTime().toMSecsSinceEpoch());
   });
   return records;
}

std::vector<std::shared_ptr<Chat::MessageData> > ChatDB::getRoomMessages(const QString& roomId)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT sender, receiver, id, timestamp, enctext, state FROM messages "\
      "WHERE (receiver=:roomid);"))) {
      logger_->error("[ChatDB::getRoomMessages] failed to prepare query: {}", query.lastError().text().toStdString());
      return {};
   }
   query.bindValue(QString::fromStdString(":roomid"), roomId);
   if (!query.exec()) {
      logger_->error("[ChatDB::getRoomMessages] failed to exec query: {}", query.lastError().text().toStdString());
      return {};
   }

   std::vector<std::shared_ptr<Chat::MessageData>> records;
   while (query.next()) {
      const auto msg = std::make_shared<Chat::MessageData>(query.value(0).toString()
         , query.value(1).toString(), query.value(2).toString(), query.value(3).toDateTime()
         , query.value(4).toString(), query.value(5).toInt());
      records.push_back(msg);
   }
   std::sort(records.begin(), records.end(), [](const std::shared_ptr<Chat::MessageData> &a
      , const std::shared_ptr<Chat::MessageData> &b) {
      return (a->dateTime().toMSecsSinceEpoch() < b->dateTime().toMSecsSinceEpoch());
   });
   return records;
}

bool ChatDB::removeRoomMessages(const QString &roomId) {
   if (!isRoomMessagesExist(roomId)) {
      return false;
   }

   QSqlQuery query(QLatin1String(
      "DELETE FROM messages WHERE receiver=:room_id;"), db_);
   query.bindValue(0, roomId);

   if (!query.exec()) {
      logger_->error("[ChatDB::removeContact] failed to delete contact.");
      return false;
   }

   return true;
}

bool ChatDB::loadKeys(std::map<QString, autheid::PublicKey>& keys_out)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT user_id, key FROM user_keys"))) {
      logger_->error("[ChatDB::loadKeys] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }
   if (!query.exec()) {
      logger_->error("[ChatDB::loadKeys] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   while (query.next()) {
      keys_out[query.value(0).toString()] = Chat::publicKeyFromString(
         query.value(1).toString().toStdString());
   }
   return true;
}

bool ChatDB::addKey(const QString& user, const autheid::PublicKey& key)
{
   QSqlQuery qryAdd(QLatin1String(
      "INSERT INTO user_keys(user_id, key) VALUES(?, ?);"), db_);
   qryAdd.bindValue(0, user);
   qryAdd.bindValue(1, QString::fromStdString(Chat::publicKeyToString(key)));

   if (!qryAdd.exec()) {
      logger_->error("[ChatDB::addKey] failed to insert new public key value to user_keys.");
      return false;
   }
   return true;
}

bool ChatDB::isContactExist(const QString &userId)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT user_id FROM contacts WHERE user_id=:user_id;"))) {
      logger_->error("[ChatDB::isContactExist] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }
   query.bindValue(QLatin1String(":user_id"), userId);

   if (!query.exec()) {
      logger_->error("[ChatDB::isContactExist] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   if (query.next()) {
      return true;
   }

   return false;
}

bool ChatDB::addContact(Chat::ContactRecordData &contact)
{
   if (isContactExist(contact.getContactForId())) {
      return true;
   }

   QSqlQuery query(db_);
   query.prepare(QLatin1String("INSERT INTO contacts(user_id, user_name, status) VALUES(:user_id, :user_name, :status)"));

   query.bindValue(QLatin1String(":user_id"), contact.getContactForId());
   query.bindValue(QLatin1String(":user_name"), contact.getDisplayName());
   query.bindValue(QLatin1String(":status"), static_cast<int>(contact.getContactStatus()));

   if (!query.exec()) {
      logger_->error("[ChatDB::addContact] failed to insert new contact: {}", query.lastError().text().toStdString());
      return false;
   }

   return true;
}

bool ChatDB::removeContact(const QString &userId)
{
   if (!isContactExist(userId)) {
      return true;
   }

   QSqlQuery query(QLatin1String(
      "DELETE FROM contacts WHERE user_id=?;"), db_);
   query.bindValue(0, userId);

   if (!query.exec()) {
      logger_->error("[ChatDB::removeContact] failed to delete contact.");
      return false;
   }

   return true;
}

bool ChatDB::getContacts(ContactRecordDataList &contactList)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT user_id, user_name, status FROM contacts;"))) {
      logger_->error("[ChatDB::getContacts] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }
   if (!query.exec()) {
      logger_->error("[ChatDB::getContacts] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   while (query.next()) {
      Chat::ContactRecordData contact(query.value(0).toString(),
                                      query.value(0).toString(),
                                      static_cast<Chat::ContactStatus>(query.value(2).toInt()),
                                      autheid::PublicKey(),
                                      query.value(1).toString());
      contactList.emplace_back(contact);
   }
   return true;
}

bool ChatDB::updateContact(Chat::ContactRecordData &contact)
{
   QSqlQuery query(db_);

   if (!contact.getDisplayName().simplified().isEmpty()) {
      if (!query.prepare(QLatin1String("UPDATE contacts SET user_name=:user_name, status=:status WHERE user_id=:user_id;"))) {
         logger_->error("[ChatDB::updateContact] failed to prepare query: {}", query.lastError().text().toStdString());
         return false;
      }
      query.bindValue(QLatin1String(":user_name"), contact.getDisplayName());
      query.bindValue(QLatin1String(":status"), static_cast<int>(contact.getContactStatus()));
      query.bindValue(QLatin1String(":user_id"), contact.getContactForId());
   } else {
      if (!query.prepare(QLatin1String("UPDATE contacts SET status=:status WHERE user_id=:user_id;"))) {
         logger_->error("[ChatDB::updateContact] failed to prepare query: {}", query.lastError().text().toStdString());
         return false;
      }
      query.bindValue(QLatin1String(":status"), static_cast<int>(contact.getContactStatus()));
      query.bindValue(QLatin1String(":user_id"), contact.getContactForId());
   }

   if (!query.exec()) {
      logger_->error("[ChatDB::updateContact] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   return true;
}

bool ChatDB::getContact(const QString& userId, Chat::ContactRecordData& contact)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT user_id, user_name, status FROM contacts WHERE user_id=?;"))) {
      logger_->error("[ChatDB::getContact] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }

   query.bindValue(0, userId);

   if (!query.exec()) {
      logger_->error("[ChatDB::getContact] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   if (query.next()) {
      contact.setUserId(query.value(0).toString());
      contact.setDisplayName(query.value(1).toString());
      contact.setStatus(static_cast<Chat::ContactStatus>(query.value(2).toInt()));
      return true;
   }

   return false;
}
