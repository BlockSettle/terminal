#ifndef __NOTIFICATION_CENTER_H__
#define __NOTIFICATION_CENTER_H__

#include <atomic>
#include <deque>
#include <memory>
#include <QMetaType>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QIcon>
#include <QSystemTrayIcon>

#ifdef BS_USE_DBUS
#include "DBusNotification.h"
#endif // BS_USE_DBUS

namespace spdlog {
   class logger;
}

namespace Ui {
    class BSTerminalMainWindow;
}
class ApplicationSettings;
class QSystemTrayIcon;

namespace bs {
   namespace ui {
      enum class NotifyType {
         Unknown,
         BlockchainTX,
         CelerOrder,
         DealerQuotes,
         AuthAddress,
         BroadcastError,
         NewVersion,
         UpdateUnreadMessage,
         FriendRequest,
         OTCOrderError,
         LogOut
      };

      using NotifyMessage = QList<QVariant>;

   }  // namespace ui
}  // namespace bs
Q_DECLARE_METATYPE(bs::ui::NotifyType)
Q_DECLARE_METATYPE(bs::ui::NotifyMessage)


class NotificationResponder : public QObject
{
   Q_OBJECT
public:
   NotificationResponder(QObject *parent = nullptr) : QObject(parent) {}

public slots:
   virtual void respond(bs::ui::NotifyType, bs::ui::NotifyMessage) {}
};

class NotificationCenter : public QObject
{
   Q_OBJECT

public:
   NotificationCenter(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<ApplicationSettings> &
      , const Ui::BSTerminalMainWindow *, const std::shared_ptr<QSystemTrayIcon> &, QObject *parent = nullptr);
   ~NotificationCenter() noexcept = default;

   static void createInstance(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<ApplicationSettings> &, const Ui::BSTerminalMainWindow *
      , const std::shared_ptr<QSystemTrayIcon> &, QObject *parent = nullptr);
   static NotificationCenter *instance();
   static void destroyInstance();
   static void notify(bs::ui::NotifyType, const bs::ui::NotifyMessage &);

signals:
   void notifyEndpoint(bs::ui::NotifyType, const bs::ui::NotifyMessage &);
   void newChatMessageClick(const QString &chatId);

private:
   void enqueue(bs::ui::NotifyType, const bs::ui::NotifyMessage &);
   void addResponder(const std::shared_ptr<NotificationResponder> &);

private:
   std::shared_ptr<spdlog::logger> logger_;
   std::vector<std::shared_ptr<NotificationResponder>>   responders_;
};


class NotificationTabResponder : public NotificationResponder
{
   Q_OBJECT
public:
   NotificationTabResponder(const Ui::BSTerminalMainWindow *mainWinUi,
      std::shared_ptr<ApplicationSettings> appSettings, QObject *parent = nullptr);

   void respond(bs::ui::NotifyType, bs::ui::NotifyMessage) override;

private:
   struct TabAction {
      int   index;
      bool  checked;
      bool  enabled;
   };
   TabAction getTabActionFor(bs::ui::NotifyType, bs::ui::NotifyMessage) const;

private:
   const Ui::BSTerminalMainWindow * mainWinUi_;
   QIcon iconDot_;
   std::shared_ptr<ApplicationSettings> appSettings_;
};

class NotificationTrayIconResponder : public NotificationResponder
{
   Q_OBJECT
public:
   NotificationTrayIconResponder(const std::shared_ptr<spdlog::logger> &, const Ui::BSTerminalMainWindow *mainWinUi
      , const std::shared_ptr<QSystemTrayIcon> &trayIcon, const std::shared_ptr<ApplicationSettings> &appSettings
      , QObject *parent = nullptr);
   
   void respond(bs::ui::NotifyType, bs::ui::NotifyMessage) override;

private slots:
   void messageClicked();
#ifdef BS_USE_DBUS
   void notificationAction(const QString &action);
#endif // BS_USE_DBUS

private:
   std::shared_ptr<spdlog::logger> logger_;
   const Ui::BSTerminalMainWindow * mainWinUi_{};
   std::shared_ptr<QSystemTrayIcon>       trayIcon_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   bool  newVersionMessage_ = false;
   bool  newChatMessage_ = false;
   QString  newChatId_;

   enum NotificationMode {
      QSystemTray,
      Freedesktop
   };

   NotificationMode notifMode_;

#ifdef BS_USE_DBUS
   DBusNotification *dbus_;
#endif // BS_USE_DBUS
};

#endif // __NOTIFICATION_CENTER_H__
