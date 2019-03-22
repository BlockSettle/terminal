#ifndef CHAT_WIDGET_H
#define CHAT_WIDGET_H

#include <QWidget>
#include <QStringListModel>
#include <QScopedPointer>

#include "ChatUserListLogic.h"

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

class ChatWidget : public QWidget
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

private slots:
   void onSendButtonClicked();
   void onUserClicked(const QString& index);
   void onMessagesUpdated();
   void onLoginFailed();
   void onUsersDeleted(const std::vector<std::string> &);
   void onSearchUserReturnPressed();
   void onChatUserRemoved(const ChatUserDataPtr &);
   void onAddUserToContacts(const QString &userId);
   void onRoomClicked(const QString& roomId);
   void onAddChatRooms(const std::vector<std::shared_ptr<Chat::ChatRoomData> >& roomList);

signals:
   void LoginFailed();

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
   bool isRoom();
   void setIsRoom(bool);
   void changeState(ChatWidget::State state);

   bool eventFilter(QObject * obj, QEvent * event) override;
};

#endif // CHAT_WIDGET_H
