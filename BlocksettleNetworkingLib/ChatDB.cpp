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
            "user_id CHAR(8) PRIMARY KEY,"\
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
            "user_id CHAR(8) PRIMARY KEY,"\
            "user_name CHAR(64),"\
            "incoming_friend_request BOOLEAN);");
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
            "sender CHAR(8) NOT NULL,"\
            "receiver CHAR(32),"\
            "enctext TEXT,"\
            "state INTEGER,"\
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

bool ChatDB::add(const Chat::MessageData &msg)
{
   QSqlQuery qryAdd(QLatin1String("INSERT INTO messages(id, timestamp, sender, receiver, enctext, state, reference)"\
      " VALUES(?, ?, ?, ?, ?, ?, ?);"), db_);
   qryAdd.bindValue(0, msg.getId());
   qryAdd.bindValue(1, msg.getDateTime());
   qryAdd.bindValue(2, msg.getSenderId());
   qryAdd.bindValue(3, msg.getReceiverId());
   qryAdd.bindValue(4, msg.getMessageData());
   qryAdd.bindValue(5, msg.getState());
   qryAdd.bindValue(6, QString());

   if (!qryAdd.exec()) {
      logger_->error("[ChatDB::add] failed to insert to changed");
      return false;
   }
   return true;
}

std::vector<std::shared_ptr<Chat::MessageData>> ChatDB::getUserMessages(const QString &userId)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT sender, receiver, id, timestamp, enctext, state FROM messages "\
      "WHERE sender=:user OR receiver=:user"))) {
      logger_->error("[ChatDB::getUserMessages] failed to prepare query: {}", query.lastError().text().toStdString());
      return {};
   }
   query.bindValue(QString::fromStdString(":user"), userId);
   if (!query.exec()) {
      logger_->error("[ChatDB::getUserMessages] failed to exec query: {}", query.lastError().text().toStdString());
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
      return (a->getDateTime().toMSecsSinceEpoch() < b->getDateTime().toMSecsSinceEpoch());
   });
   return records;
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
      keys_out[query.value(0).toString()] = autheid::publicKeyFromString(
         query.value(1).toString().toStdString());
   }
   return true;
}

bool ChatDB::addKey(const QString& user, const autheid::PublicKey& key)
{
   QSqlQuery qryAdd(QLatin1String(
      "INSERT INTO user_keys(user_id, key) VALUES(?, ?);"), db_);
   qryAdd.bindValue(0, user);
   qryAdd.bindValue(1, QString::fromStdString(autheid::publicKeyToString(key)));

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
   query.bindValue(QString::fromStdString(":user_id"), userId);
   if (!query.exec()) {
      logger_->error("[ChatDB::isContactExist] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   if (query.next()) {
      return true;
   }

   return false;
}

bool ChatDB::addContact(const ContactUserData &contact)
{
   if (isContactExist(contact.userId())) {
      return false;
   }

   QSqlQuery query(QLatin1String(
      "INSERT INTO contacts(user_id, user_name, incoming_friend_request) VALUES(?, ?, ?);"), db_);
   query.bindValue(0, contact.userId());
   query.bindValue(1, contact.userName());
   query.bindValue(2, contact.incomingFriendRequest() ? 1 : 0);

   if (!query.exec()) {
      logger_->error("[ChatDB::addContact] failed to insert new contact.");
      return false;
   }

   return true;
}

bool ChatDB::removeContact(const ContactUserData &contact)
{
   if (!isContactExist(contact.userId())) {
      return false;
   }

   QSqlQuery query(QLatin1String(
      "DELETE FROM contacts WHERE user_id=:user_id;"), db_);
   query.bindValue(0, contact.userId());

   if (!query.exec()) {
      logger_->error("[ChatDB::removeContact] failed to delete contact.");
      return false;
   }

   return true;
}

bool ChatDB::getContacts(ContactUserDataList &contactList)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT user_id, user_name, incoming_friend_request FROM contacts;"))) {
      logger_->error("[ChatDB::getContacts] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }
   if (!query.exec()) {
      logger_->error("[ChatDB::getContacts] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   while (query.next()) {
      ContactUserData contact;
      contact.setUserId(query.value(0).toString());
      contact.setUserName(query.value(1).toString());
      contact.setIncomingFriendRequest(query.value(2).toBool());
      contactList.emplace_back(contact);
   }
   return true;
}

bool ChatDB::updateContact(const ContactUserData &contact)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("UPDATE contacts SET user_name=:user_name, incoming_friend_request=:incoming_friend_request WHERE user_id=:user_id;"))) {
      logger_->error("[ChatDB::getContacts] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }
   query.bindValue(QString::fromStdString(":user_id"), contact.userId());
   query.bindValue(QString::fromStdString(":user_name"), contact.userName());
   query.bindValue(QString::fromStdString(":incoming_friend_request"), contact.incomingFriendRequest() ? 1 : 0);
   if (!query.exec()) {
      logger_->error("[ChatDB::updateContact] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   return true;
}
