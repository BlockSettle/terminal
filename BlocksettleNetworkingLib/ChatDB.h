#ifndef __CHAT_DB_H__
#define __CHAT_DB_H__

#include <functional>
#include <map>
#include <memory>
#include <QObject>
#include <QStringList>
#include <QtSql/QSqlDatabase>
#include "BinaryData.h"
#include "ChatProtocol.h"

namespace spdlog {
   class logger;
}


class ChatDB : public QObject
{
   Q_OBJECT
public:
   ChatDB(const std::shared_ptr<spdlog::logger> &logger, const QString &dbFile);
   ~ChatDB() noexcept = default;

   ChatDB(const ChatDB&) = delete;
   ChatDB& operator = (const ChatDB&) = delete;
   ChatDB(ChatDB&&) = delete;
   ChatDB& operator = (ChatDB&&) = delete;

   bool add(const Chat::MessageData &);

   std::vector<std::shared_ptr<Chat::MessageData>> getUserMessages(const QString &userId);

   /** Adds given username->publickey pair to DB.
    * \param[in] user Chat user name, currently a base64 encoded hash or PK.
    * \param[in] key Public key of the user.
    * \returns false of failure, otherwise true.
    */
   bool addKey(const QString& user, const autheid::PublicKey& key);

   bool loadKeys(std::map<QString, autheid::PublicKey>& peer_public_keys_out);

   bool isContactExist(const QString &user_name);
   bool addContact(const QString &user_name);
   bool removeContact(const QString &user_name);

private:
   bool createMissingTables();

private:
   std::shared_ptr<spdlog::logger>     logger_;
   QSqlDatabase                        db_;
   const QStringList                   requiredTables_;

   using createTableFunc = std::function<bool(void)>;
   std::map<QString, createTableFunc>  createTable_;
};

#endif // __CHAT_DB_H__
