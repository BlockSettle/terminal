#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QStringListModel>
#include <QScopedPointer>


namespace Ui {
    class ChatWidget;
}


namespace spdlog {
    class logger;
}


class ChatClient;
class ChatServer;
class ConnectionManager;
class ApplicationSettings;


class ChatWidget : public QWidget
{
    Q_OBJECT


private:

    QScopedPointer<Ui::ChatWidget> ui;

    std::shared_ptr<ChatClient> client_;

    std::shared_ptr<ChatServer> server_;

    std::shared_ptr<spdlog::logger> logger_;

    std::string serverPublicKey_;


public:

    explicit ChatWidget(QWidget *parent = nullptr);
    ~ChatWidget();

    void init(const std::shared_ptr<ConnectionManager>& connectionManager
              , const std::shared_ptr<ApplicationSettings> &appSettings
              , const std::shared_ptr<spdlog::logger>& logger);

    void setUserName(const QString& username);

    void setUserId(const QString& userId);


public slots:

    void addMessage(const QString &txt);
    void addUser(const QString &txt);
    void addGroup(const QString &txt);

};

#endif // CHATWIDGET_H
