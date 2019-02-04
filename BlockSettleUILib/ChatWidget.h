#ifndef __CHAT_WIDGET_H__
#define __CHAT_WIDGET_H__

#include <QWidget>
#include <QStringListModel>
#include <QScopedPointer>

#include "ChatUsersViewModel.h"
#include "ChatMessagesViewModel.h"


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

private slots:
   void onSendButtonClicked();
   void onUserClicked(const QModelIndex& index);
   void onMessagesUpdated(const QModelIndex& parent, int start, int end);
   void onLoginFailed();
   void onUsersDeleted(const std::vector<std::string> &);
   void onSearchUserReturnPressed();

signals:
   void LoginFailed();

private:
   QScopedPointer<Ui::ChatWidget> ui_;
   QScopedPointer<ChatUsersViewModel> usersViewModel_;
   QScopedPointer<ChatMessagesViewModel> messagesViewModel_;


   std::shared_ptr<ChatClient>      client_;
   std::shared_ptr<spdlog::logger>  logger_;

   std::string serverPublicKey_;
   QString  currentChat_;
   ChatSearchPopup *popup_;

private:
   std::shared_ptr<ChatWidgetState> stateCurrent_;

private:
   void changeState(ChatWidget::State state);

    bool eventFilter(QObject * obj, QEvent * event) override;

};

#endif // __CHAT_WIDGET_H__
