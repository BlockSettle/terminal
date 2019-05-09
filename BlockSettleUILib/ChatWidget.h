#ifndef CHAT_WIDGET_H
#define CHAT_WIDGET_H

#include <QWidget>
#include <QStringListModel>
#include <QScopedPointer>
#include <QLayoutItem>

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
}

class ChatClient;
class ConnectionManager;
class ApplicationSettings;
class ChatWidgetState;
class ChatSearchPopup;
class OTCRequestViewModel;

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

public slots:
   void onLoggedOut();
   void onNewChatMessageTrayNotificationClicked(const QString &chatId);

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

   void OnOTCRequestCreated();
   void DisplayOTCRequest(const bs::network::Side::Type& side, const bs::network::OTCRangeID& range);

   void OnOTCResponseCreated();

signals:
   void LoginFailed();
   void LogOut();

private:
   void SetOTCLoggedInState();
   void SetLoggedOutOTCState();

   void OTCSwitchToCommonRoom();
   void OTCSwitchToDMRoom();
   void OTCSwitchToGlobalRoom();

private:
   QScopedPointer<Ui::ChatWidget> ui_;

   std::shared_ptr<ChatClient>      client_;
   std::shared_ptr<spdlog::logger>  logger_;

   std::string serverPublicKey_;
   QString  currentChat_;
   ChatSearchPopup *popup_;
   bool isRoom_;
   QSpacerItem *chatUsersVerticalSpacer_;
   QTimer *popupVisibleTimer_;

private:
   std::shared_ptr<ChatWidgetState> stateCurrent_;
   QMap<QString, QString> draftMessages_;
   bool needsToStartFirstRoom_;

private:
   OTCRequestViewModel *otcRequestViewModel_ = nullptr;

private:
   bool isRoom();
   void setIsRoom(bool);
   void changeState(ChatWidget::State state);
   void initPopup();
   void setPopupVisible(const bool &value);

   bool eventFilter(QObject * obj, QEvent * event) override;

   // ViewItemWatcher interface
public:
   void onElementSelected(CategoryElement *element) override;
   void onMessageChanged(std::shared_ptr<Chat::MessageData> message) override;
   void onElementUpdated(CategoryElement *element) override;

   // NewMessageMonitor interface
public:
   void onNewMessagePresent(const bool isNewMessagePresented, std::shared_ptr<Chat::MessageData> message) override;
};









#endif // CHAT_WIDGET_H
