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


class ChatWidget : public QWidget
{
   Q_OBJECT


private:

   QScopedPointer<Ui::ChatWidget> ui_;
   QScopedPointer<ChatUsersViewModel> usersViewModel_;
   QScopedPointer<ChatMessagesViewModel> messagesViewModel_;

   std::shared_ptr<ChatClient>      client_;
   std::shared_ptr<spdlog::logger>  logger_;

   std::string serverPublicKey_;


public:

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

};

#endif // __CHAT_WIDGET_H__
