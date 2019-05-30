#pragma once

#include <memory>

#include <QObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "EncryptionUtils.h"
#include "autheid_utils.h"

namespace Chat
{
   enum class RequestType
   {
       RequestHeartbeatPing
   ,   RequestLogin
   ,   RequestLogout
   ,   RequestSendMessage
   ,   RequestMessages
   ,   RequestOnlineUsers
   ,   RequestAskForPublicKey
   ,   RequestSendOwnPublicKey
   ,   RequestChangeMessageStatus
   ,   RequestContactsActionDirect
   ,   RequestContactsActionServer
   ,   RequestChatroomsList
   ,   RequestSendRoomMessage
   ,   RequestContactsList
   ,   RequestSearchUsers
   };


   enum class ResponseType
   {
       ResponseHeartbeatPong
   ,   ResponseLogin
   ,   ResponseMessages
   ,   ResponseSuccess
   ,   ResponseError
   ,   ResponseUsersList
   ,   ResponseAskForPublicKey
   ,   ResponseSendOwnPublicKey
   ,   ResponsePendingMessage
   ,   ResponseSendMessage
   ,   ResponseChangeMessageStatus
   ,   ResponseContactsActionDirect
   ,   ResponseContactsActionServer
   ,   ResponseChatroomsList
   ,   ResponseRoomMessages
   ,   ResponseContactsList
   ,   ResponseSearchUsers
   ,   ResponseLogout
   };
   
   enum class ContactsAction {
      Accept,
      Reject,
      Request,
      Remove
   };

   enum class ContactStatus {
      Accepted,
      Rejected,
      Incoming,
      Outgoing
   };

   enum class UserStatus {
       Online,
       Offline
   };

   enum class ContactsActionServer {
       AddContactRecord,
       RemoveContactRecord,
       UpdateContactRecord
   };

   enum class ContactsActionServerResult {
       Success,
       Failed
   };
   
   static const QString VersionKey   = QStringLiteral("version");
   static const QString NameKey      = QStringLiteral("name");
   static const QString TypeKey      = QStringLiteral("type");
   static const QString TextKey      = QStringLiteral("text");
   static const QString RoomKey      = QStringLiteral("room");
   static const QString RoomsKey     = QStringLiteral("rooms");
   static const QString MessageKey   = QStringLiteral("message");
   static const QString FromKey      = QStringLiteral("from");
   static const QString ContactsKey  = QStringLiteral("fromid");
   static const QString IdKey        = QStringLiteral("id");
   static const QString AuthIdKey    = QStringLiteral("authid");
   static const QString JwtKey       = QStringLiteral("jwt");
   static const QString PasswordKey  = QStringLiteral("passwd");
   static const QString ReceiverIdKey  = QStringLiteral("toid");
   static const QString SenderIdKey  = QStringLiteral("fromid");
   static const QString StatusKey    = QStringLiteral("status");
   static const QString UsersKey     = QStringLiteral("users");
   static const QString DateTimeKey  = QStringLiteral("datetm");
   static const QString DataKey      = QStringLiteral("data");
   static const QString PublicKeyKey = QStringLiteral("public_key");
   static const QString CommandKey = QStringLiteral("cmd");
   static const QString MessageIdKey = QStringLiteral("message_id");
   static const QString ClientMessageIdKey = QStringLiteral("client_message_id");
   static const QString MessageResultKey = QStringLiteral("message_result");
   static const QString ContactIdKey = QStringLiteral("contact_id");
   static const QString MessageStateKey = QStringLiteral("message_state");
   static const QString ContactActionKey = QStringLiteral("contacts_action");
   static const QString ContactActionResultKey = QStringLiteral("contacts_action_result");
   static const QString ContactActionResultMessageKey = QStringLiteral("contacts_action_result_message");
   static const QString ContactStatusKey = QStringLiteral("contact_status");
   static const QString RoomKeyKey = QStringLiteral("room_id");
   static const QString RoomTitleKey = QStringLiteral("room_title");
   static const QString RoomOwnerIdKey = QStringLiteral("room_owner_id");
   static const QString RoomIsPrivateKey = QStringLiteral("room_is_private");
   static const QString RoomSendUserUpdatesKey = QStringLiteral("room_send_user_updates");
   static const QString RoomDisplayUserListKey = QStringLiteral("room_display_user_list");
   static const QString RoomDisplayTrayNotificationKey = QStringLiteral("room_display_tray_notification");
   static const QString UserIdKey = QStringLiteral("user_id");
   static const QString DisplayNameKey = QStringLiteral("display_name");
   static const QString SearchIdPatternKey = QStringLiteral("search_id_pattern");
   static const QString UserStatusKey = QStringLiteral("user_status");
   static const QString Nonce = QStringLiteral("nonce");
   static const QString EncryptionTypeKey = QStringLiteral("encryption_type");
   static const QString GlobalRoomKey = QStringLiteral("global_chat");
   static const QString OTCRoomKey = QStringLiteral("otc_chat");

   template <typename T>
   class Message
   {
   public:

      Message(T messageType)
         : messageType_(messageType)
         , version_("1.0.0")
      {
      }

      virtual ~Message() = default;
      std::string getVersion() const { return version_; }
      virtual QJsonObject toJson() const;

   protected:
      T messageType_;
      std::string version_;
   };
   
   template <typename T>
   std::string serializeData(const T* thisPtr)
   {
      auto data = QJsonDocument(thisPtr->toJson());
      QString serializedData = QString::fromUtf8(data.toJson());
      return serializedData.toStdString();
   } 
   
   autheid::PublicKey publicKeyFromString(const std::string &s);
   std::string publicKeyToString(const autheid::PublicKey &k);

} //namespace Chat

Q_DECLARE_METATYPE(Chat::ContactStatus)
Q_DECLARE_METATYPE(Chat::UserStatus)


