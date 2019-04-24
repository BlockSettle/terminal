#ifndef CHAT_WIDGET_H
#define CHAT_WIDGET_H

#include <QWidget>
#include <QStringListModel>
#include <QScopedPointer>

#include "ChatUserListLogic.h"
#include "ChatHandleInterfaces.h"
#include "CommonTypes.h"

#include <memory>

namespace Ui {
   class ChatWidget;
}
namespace spdlog {
   class logger;
}

class ChatClient;
class ConnectionManager;
class ApplicationSettings;
class ChatWidgetState;
class ChatSearchPopup;
class OTCRequestViewModel;

class ChatWidget : public QWidget, public ViewItemWatcher
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

   std::string login(const std::string& email, const std::string& jwt);
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
   void onSearchUserReturnPressed();
   void onChatUserRemoved(const ChatUserDataPtr &);
   void onSendFriendRequest(const QString &userId);
   void onAcceptFriendRequest(const QString &userId);
   void onDeclineFriendRequest(const QString &userId);
   void onAddChatRooms(const std::vector<std::shared_ptr<Chat::RoomData> >& roomList);
   void onSearchUserListReceived(const std::vector<std::shared_ptr<Chat::UserData>>& users);

   void OnOTCRequestCreated();
   void DisplayOTCRequest(const bs::network::Side::Type& side, const bs::network::OTCRangeID& range);

   void OnOTCResponseCreated();

signals:
   void LoginFailed();
   void LogOut();

private:
   QScopedPointer<Ui::ChatWidget> ui_;

   std::shared_ptr<ChatClient>      client_;
   std::shared_ptr<spdlog::logger>  logger_;

   std::string serverPublicKey_;
   QString  currentChat_;
   ChatSearchPopup *popup_;
   bool isRoom_;

private:
   std::shared_ptr<ChatWidgetState> stateCurrent_;
   ChatUserListLogicPtr chatUserListLogicPtr_;
   QMap<QString, QString> draftMessages_;
   bool needsToStartFirstRoom_;

private:
   OTCRequestViewModel *otcRequestViewModel_ = nullptr;

private:
   bool isRoom();
   void setIsRoom(bool);
   void changeState(ChatWidget::State state);

   bool eventFilter(QObject * obj, QEvent * event) override;

   // ViewItemWatcher interface
public:
   void onElementSelected(CategoryElement *element) override;

   // ViewItemWatcher interface
public:
   void onMessageChanged(std::shared_ptr<Chat::MessageData> message) override;

   // ViewItemWatcher interface
public:
   void onElementUpdated(CategoryElement *element) override;
};







#endif // CHAT_WIDGET_H
