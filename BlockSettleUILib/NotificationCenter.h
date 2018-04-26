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
   NotificationCenter(const std::shared_ptr<ApplicationSettings> &, const Ui::BSTerminalMainWindow *
      , const std::shared_ptr<QSystemTrayIcon> &, QObject *parent = nullptr);
   ~NotificationCenter() noexcept = default;

   static void createInstance(const std::shared_ptr<ApplicationSettings> &, const Ui::BSTerminalMainWindow *
      , const std::shared_ptr<QSystemTrayIcon> &, QObject *parent = nullptr);
   static void destroyInstance();
   static void notify(bs::ui::NotifyType, const bs::ui::NotifyMessage &);

signals:
   void notifyEndpoint(bs::ui::NotifyType, bs::ui::NotifyMessage);

private:
   void enqueue(bs::ui::NotifyType, const bs::ui::NotifyMessage &);
   void addResponder(const std::shared_ptr<NotificationResponder> &);

private:
   std::vector<std::shared_ptr<NotificationResponder>>   responders_;
};


class NotificationTabResponder : public NotificationResponder
{
   Q_OBJECT
public:
   NotificationTabResponder(const Ui::BSTerminalMainWindow *mainWinUi, QObject *parent = nullptr);

   void respond(bs::ui::NotifyType, bs::ui::NotifyMessage) override;

private:
   struct TabAction {
      int   index;
      bool  checked;
   };
   TabAction getTabActionFor(bs::ui::NotifyType, bs::ui::NotifyMessage) const;

private:
   const Ui::BSTerminalMainWindow * mainWinUi_;
   QIcon iconDot_;
};

class NotificationTrayIconResponder : public NotificationResponder
{
   Q_OBJECT
public:
   NotificationTrayIconResponder(const std::shared_ptr<QSystemTrayIcon> &trayIcon, const std::shared_ptr<ApplicationSettings> &appSettings
      , QObject *parent = nullptr);
   
   void respond(bs::ui::NotifyType, bs::ui::NotifyMessage) override;

private slots:
   void newVersionMessageClicked();

private:
   std::shared_ptr<QSystemTrayIcon>       trayIcon_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   bool  newVersionMessage_ = false;
};

#endif // __NOTIFICATION_CENTER_H__
