#ifndef __CHAT_DB_H__
#define __CHAT_DB_H__

#include <functional>
#include <map>
#include <memory>
#include <QObject>
#include <QStringList>
#include <QtSql/QSqlDatabase>
#include "BinaryData.h"
#include "chat.pb.h"

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

using ContactRecordDataList = std::vector<Chat::Data_ContactRecord>;


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

   bool add(const std::shared_ptr<Chat::Data>&);
   bool syncMessageId(const std::string& localId, const std::string& serverId);
   bool updateMessageStatus(const std::string& messageId, int ustatus);

   std::vector<std::shared_ptr<Chat::Data>> getUserMessages(const std::string &ownUserId, const std::string &userId);
   std::vector<std::shared_ptr<Chat::Data>> getRoomMessages(const std::string &roomId);
   bool removeRoomMessages(const std::string &roomId);
   bool isRoomMessagesExist(const std::string &userId);

   /** Adds given username->publickey pair to DB.
    * \param[in] user Chat user name, currently a base64 encoded hash or PK.
    * \param[in] key Public key of the user.
    * \returns false of failure, otherwise true.
    */
   bool addKey(const std::string& userId, const BinaryData& key, const QDateTime& dt);
   bool removeKey(const std::string& userId);

   std::map<std::string, BinaryData> loadKeys(bool* loaded = nullptr);

   bool isContactExist(const std::string &userId);
   bool addContact(Chat::Data &contact);
   bool removeContact(const std::string &userId);
   bool getContacts(ContactRecordDataList &contactList);
   bool updateContact(Chat::Data &contact);
   bool getContact(const std::string &userId, Chat::Data_ContactRecord *contact);
   bool compareLocalData(const std::string& userId, const BinaryData& key, const QDateTime& dt);
   bool updateContactKey(const std::string& userId, const BinaryData& publicKey, const QDateTime& publicKeyTimestamp);

//   bool insertContactRecord(const std::shared_ptr<Chat::Data_ContactRecord> contact);
//   bool removeContactRecord(const std::shared_ptr<Chat::Data_ContactRecord> contact);
//   bool updateContactRecord(const std::shared_ptr<Chat::Data_ContactRecord> contact);
//   std::vector<std::shared_ptr<Chat::Data_ContactRecord>> getContactRecordList(const QString userdId);

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
