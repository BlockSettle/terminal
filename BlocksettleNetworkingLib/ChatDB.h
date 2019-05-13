#ifndef __CHAT_DB_H__
#define __CHAT_DB_H__

#include <functional>
#include <map>
#include <memory>
#include <QObject>
#include <QStringList>
#include <QtSql/QSqlDatabase>
#include "BinaryData.h"
#include "ChatProtocol/ChatProtocol.h"

namespace spdlog {
   class logger;
}

class ContactUserData
{
public:
   enum class Status {
      Friend = 0,
      Rejected,
      Incoming,
      Outgoing
   };
   QString userName() const { return _userName; }
   void setUserName(const QString &userName) { _userName = userName; }

   QString userId() const { return _userId; }
   void setUserId(const QString &userId) { _userId = userId; }

   Status status() const {return status_;}
   void setStatus(Status status) { status_ = status;}

private:
   QString _userName;
   QString _userId;
   Status status_;

};

using ContactRecordDataList = std::vector<Chat::ContactRecordData>;


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
   bool syncMessageId(const QString& localId, const QString& serverId);
   bool updateMessageStatus(const QString& messageId, int ustatus);

   std::vector<std::shared_ptr<Chat::MessageData>> getUserMessages(const QString &ownUserId, const QString &userId);
   std::vector<std::shared_ptr<Chat::MessageData>> getRoomMessages(const QString &roomId);
   bool removeRoomMessages(const QString &roomId);
   bool isRoomMessagesExist(const QString &userId);

   /** Adds given username->publickey pair to DB.
    * \param[in] user Chat user name, currently a base64 encoded hash or PK.
    * \param[in] key Public key of the user.
    * \returns false of failure, otherwise true.
    */
   bool addKey(const QString& user, const autheid::PublicKey& key);

   bool loadKeys(std::map<QString, autheid::PublicKey>& peer_public_keys_out);

   bool isContactExist(const QString &userId);
   bool addContact(Chat::ContactRecordData &contact);
   bool removeContact(const QString &userId);
   bool getContacts(ContactRecordDataList &contactList);
   bool updateContact(Chat::ContactRecordData &contact);
   bool getContact(const QString& userId, Chat::ContactRecordData& contact);

//   bool insertContactRecord(const std::shared_ptr<Chat::ContactRecordData> contact);
//   bool removeContactRecord(const std::shared_ptr<Chat::ContactRecordData> contact);
//   bool updateContactRecord(const std::shared_ptr<Chat::ContactRecordData> contact);
//   std::vector<std::shared_ptr<Chat::ContactRecordData>> getContactRecordList(const QString userdId);

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
