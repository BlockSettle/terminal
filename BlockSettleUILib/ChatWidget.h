#ifndef CHAT_WIDGET_H
#define CHAT_WIDGET_H

#include <QItemSelection>
#include <QLayoutItem>
#include <QScopedPointer>
#include <QStringListModel>
#include <QWidget>

#include "ChatHandleInterfaces.h"
#include "CommonTypes.h"
#include "ZMQ_BIP15X_DataConnection.h"

#include <memory>

namespace Ui {
   class ChatWidget;
}
namespace spdlog {
   class logger;
}

namespace Chat {
   class RoomData;
   class UserData;
   class OTCResponseData;
   class OTCRequestData;
}

class ChatClient;
class ConnectionManager;
class ApplicationSettings;
class ChatWidgetState;
class OTCRequestViewModel;
class ChatTreeModelWrapper;
class CelerClient;

class ChatWidget : public QWidget
                 , public ViewItemWatcher
                 , public NewMessageMonitor
{
   Q_OBJECT

public:
   enum State {
      LoggedIn,
      LoggedOut
   };
   //friend class ChatWidgetState;
   friend class ChatWidgetStateLoggedOut;
   friend class ChatWidgetStateLoggedIn;

   explicit ChatWidget(QWidget *parent = nullptr);
   ~ChatWidget() override;

   void init(const std::shared_ptr<ConnectionManager>& connectionManager
           , const std::shared_ptr<ApplicationSettings> &appSettings
           , const std::shared_ptr<spdlog::logger>& logger);

   std::string login(const std::string& email, const std::string& jwt
      , const ZmqBIP15XDataConnection::cbNewKey &);
   void logout();
   bool hasUnreadMessages();
   void switchToChat(const QString& chatId);
   void setCelerClient(std::shared_ptr<CelerClient> celerClient);
   void updateChat(const bool &isChatTab);

public slots:
   void onLoggedOut();
   void onNewChatMessageTrayNotificationClicked(const QString &userId);

private slots:
   void onSendButtonClicked();
   void onUserClicked(const QString& index);
   void onRoomClicked(const QString& roomId);
   void onMessagesUpdated();
   void onLoginFailed();
   void onUsersDeleted(const std::vector<std::string> &);
   void onSendFriendRequest(const QString &userId);
   void onRemoveFriendRequest(const QString &userId);
   void onAddChatRooms(const std::vector<std::shared_ptr<Chat::RoomData> >& roomList);
   void onSearchUserListReceived(const std::vector<std::shared_ptr<Chat::UserData>>& users);
   void onSearchUserTextEdited(const QString& text);
   void onConnectedToServer();
   void selectGlobalRoom();
   void onContactRequestAccepted(const QString &userId);
   void onBSChatInputSelectionChanged();
   void onChatMessagesSelectionChanged();

   // OTC UI slots
   void OnOTCRequestCreated();
   void OnOTCResponseCreated();

   void OnPullOwnOTCRequest(const QString& otcId);

   void OnOTCSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

   // OTC chat client slots
   void OnOTCRequestAccepted(const std::shared_ptr<Chat::OTCRequestData>& otcRequest);
   void OnOTCOwnRequestRejected(const QString& reason);
   void OnNewOTCRequestReceived(const std::shared_ptr<Chat::OTCRequestData>& otcRequest);
   void OnOTCRequestCancelled(const std::string& otcId);
   void OnOTCRequestExpired(const std::string& otcId);
   void OnOwnOTCRequestExpired(const std::string& otcId);

signals:
   void LoginFailed();
   void LogOut();

private:
   void SetOTCLoggedInState();
   void SetLoggedOutOTCState();

   void OTCSwitchToCommonRoom();
   void OTCSwitchToDMRoom();
   void OTCSwitchToGlobalRoom();
   void OTCSwitchToRoom(std::shared_ptr<Chat::RoomData>& room);
   void OTCSwitchToContact(std::shared_ptr<Chat::ContactRecordData>& contact, bool onlineStatus);
   void OTCSwitchToResponse(std::shared_ptr<Chat::OTCResponseData>& response);


   // used to display proper widget if OTC room selected.
   // either create OTC or Pull OTC, if was submitted
   void DisplayCorrespondingOTCRequestWidget();

   bool IsOTCRequestSubmitted() const;
   bool IsOTCRequestAccepted() const;

   void DisplayCreateOTCWidget();
   void DisplayOwnSubmittedOTC();
   void DisplayOwnLiveOTC();

   bool IsOwnOTCId(const std::string& otcId) const;
   void OnOwnOTCPulled();
   void OnOTCCancelled(const std::string& otcId);

   bool IsOTCChatSelected() const;
   void UpdateOTCRoomWidgetIfRequired();

private:
   QScopedPointer<Ui::ChatWidget> ui_;

   std::shared_ptr<ChatClient>      client_;
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<CelerClient>     celerClient_;

   std::string serverPublicKey_;
   QString  currentChat_;
   bool isRoom_;
   QSpacerItem *chatUsersVerticalSpacer_;
   bool isChatMessagesSelected_;
   bool isChatTab_;

private:
   std::shared_ptr<ChatWidgetState> stateCurrent_;
   QMap<QString, QString> draftMessages_;
   bool needsToStartFirstRoom_;

private:
   OTCRequestViewModel *otcRequestViewModel_ = nullptr;

   bool                          otcSubmitted_ = false;
   bs::network::OTCRequest       submittedOtc_;

   bool                          otcAccepted_ = false;
   std::shared_ptr<Chat::OTCRequestData>   ownActiveOTC_;

private:
   bool isRoom();
   void setIsRoom(bool);
   void changeState(ChatWidget::State state);
   void initSearchWidget();

   bool eventFilter(QObject *sender, QEvent *event) override;

   // ViewItemWatcher interface
public:
   void onElementSelected(CategoryElement *element) override;
   void onMessageChanged(std::shared_ptr<Chat::MessageData> message) override;
   void onElementUpdated(CategoryElement *element) override;

   // NewMessageMonitor interface
public:
   void onNewMessagesPresent(std::map<QString, std::shared_ptr<Chat::MessageData>> newMessages) override;
};

#endif // CHAT_WIDGET_H
