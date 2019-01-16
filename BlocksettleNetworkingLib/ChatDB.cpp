#include "ChatDB.h"
#include <set>
#include <spdlog/spdlog.h>
#include <QDateTime>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QVariant>


ChatDB::ChatDB(const std::shared_ptr<spdlog::logger> &logger, const QString &dbFile)
   : logger_(logger)
   , requiredTables_({QLatin1String("user_keys"), QLatin1String("room_keys"), QLatin1String("messages")})
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
            "user CHAR(8) PRIMARY KEY,"\
            "key TEXT"\
            ");");
         if (!QSqlQuery(db).exec(query)) {
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

bool ChatDB::loadKeys(std::map<QString, BinaryData> &keys)
{
   QSqlQuery query(db_);
   if (!query.prepare(QLatin1String("SELECT user, key FROM user_keys"))) {
      logger_->error("[ChatDB::loadKeys] failed to prepare query: {}", query.lastError().text().toStdString());
      return false;
   }
   if (!query.exec()) {
      logger_->error("[ChatDB::loadKeys] failed to exec query: {}", query.lastError().text().toStdString());
      return false;
   }

   while (query.next()) {  //TODO: apply base64 decoding for public key
      keys[query.value(0).toString()] = query.value(1).toString().toStdString();
   }
   return true;
}
