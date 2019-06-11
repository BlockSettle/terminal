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
   ,   RequestGenCommonOTC
   ,   RequestAnswerCommonOTC
   ,   RequestUpdateCommonOTC
   ,   RequestPullOTC
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
   ,   ResponseGenCommonOTC
   ,   ResponseAnswerCommonOTC
   ,   ResponseUpdateCommonOTC
   };

   enum class ContactsAction {
      Accept,
      Reject,
      Request,
      Remove
   };

   enum class ContactStatus {
      Accepted,
      Incoming,
      Outgoing,
      Rejected
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

   enum class OTCResult {
      Accepted,
      Rejected,
      Canceled,
      Expired,
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
   static const QString RoomIsTradingAvailableKey = QStringLiteral("room_is_trading_available");
   static const QString UserIdKey = QStringLiteral("user_id");
   static const QString DisplayNameKey = QStringLiteral("display_name");
   static const QString SearchIdPatternKey = QStringLiteral("search_id_pattern");
   static const QString UserStatusKey = QStringLiteral("user_status");
   static const QString Nonce = QStringLiteral("nonce");
   static const QString EncryptionTypeKey = QStringLiteral("encryption_type");
   static const QString GlobalRoomKey = QStringLiteral("global_chat");
   static const QString OTCRoomKey = QStringLiteral("otc_chat");
   static const QString OTCDataObjectKey = QStringLiteral("otc_data");
   static const QString OTCRqSideKey = QStringLiteral("otc_rq_side");
   static const QString OTCRqRangeIdKey = QStringLiteral("otc_rq_range");
   static const QString OTCRequestIdClientKey = QStringLiteral("otc_request_id_client");
   static const QString OTCRequestIdServerKey = QStringLiteral("otc_request_id_server");
   static const QString OTCResponseIdClientKey = QStringLiteral("otc_response_id_client");
   static const QString OTCResponseIdServerKey = QStringLiteral("otc_response_id_server");
   static const QString OTCUpdateIdClientKey = QStringLiteral("otc_update_id_clientr");
   static const QString OTCUpdateIdServerKey = QStringLiteral("otc_update_id_server");
   static const QString OTCRequestorIdKey = QStringLiteral("otc_requestor_id");
   static const QString OTCResponderIdKey = QStringLiteral("otc_responder_id");
   static const QString OTCUpdateSenderIdKey = QStringLiteral("otc_update_sender_id");
   static const QString OTCUpdateReceiverIdKey = QStringLiteral("otc_update_receiver_id");
   static const QString OTCTargetIdKey = QStringLiteral("otc_target_id");
   static const QString OTCSubmitTimestampKey = QStringLiteral("otc_submit_timestamp");
   static const QString OTCExpiredTimestampKey = QStringLiteral("otc_expired_timestamp");
   static const QString OTCResponseTimestampKey = QStringLiteral("otc_response_timestamp");
   static const QString OTCUpdateTimestampKey = QStringLiteral("otc_update_timestamp");
   static const QString OTCNegotiationChannelIdKey = QStringLiteral("otc_negotiation_channel_id");
   static const QString OTCPriceRangeObjectKey = QStringLiteral("otc_price_range");
   static const QString OTCQuantityRangeObjectKey = QStringLiteral("otc_quantity_range");
   static const QString OTCUpdateAmountKey = QStringLiteral("otc_update_amount");
   static const QString OTCUpdatePriceKey = QStringLiteral("otc_update_price");
   static const QString OTCLowerKey = QStringLiteral("lower");
   static const QString OTCUpperKey = QStringLiteral("upper");
   static const QString OTCResultKey = QStringLiteral("otc_result");
   static const QString OTCMessageKey = QStringLiteral("otc_message");



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
Q_DECLARE_METATYPE(Chat::OTCResult)


