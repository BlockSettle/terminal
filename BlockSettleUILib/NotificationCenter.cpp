#include "NotificationCenter.h"
#include "ui_BSTerminalMainWindow.h"
#include "ApplicationSettings.h"
#include "MessageBoxWarning.h"

#include <QSystemTrayIcon>
#if defined (Q_OS_WIN)
#include <shellapi.h>
#else
#include <stdlib.h>
#endif

static std::shared_ptr<NotificationCenter> globalInstance = nullptr;

NotificationCenter::NotificationCenter(const std::shared_ptr<ApplicationSettings> &appSettings, const Ui::BSTerminalMainWindow *mainWinUi
   , const std::shared_ptr<QSystemTrayIcon> &trayIcon, QObject *parent)
   : QObject(parent)
{
   qRegisterMetaType<bs::ui::NotifyType>("NotifyType");
   qRegisterMetaType<bs::ui::NotifyMessage>("NotifyMessage");

   addResponder(std::make_shared<NotificationTabResponder>(mainWinUi, this));
   addResponder(std::make_shared<NotificationTrayIconResponder>(trayIcon, appSettings, this));
}

void NotificationCenter::createInstance(const std::shared_ptr<ApplicationSettings> &appSettings, const Ui::BSTerminalMainWindow *ui
   , const std::shared_ptr<QSystemTrayIcon> &trayIcon, QObject *parent)
{
   globalInstance = std::make_shared<NotificationCenter>(appSettings, ui, trayIcon, parent);
}

void NotificationCenter::destroyInstance()
{
   globalInstance = nullptr;
}

void NotificationCenter::notify(bs::ui::NotifyType nt, const bs::ui::NotifyMessage &msg)
{
   if (!globalInstance) {
      return;
   }
   globalInstance->enqueue(nt, msg);
}

void NotificationCenter::enqueue(bs::ui::NotifyType nt, const bs::ui::NotifyMessage &msg)
{
   emit notifyEndpoint(nt, msg);
}

void NotificationCenter::addResponder(const std::shared_ptr<NotificationResponder> &responder)
{
   connect(this, &NotificationCenter::notifyEndpoint, responder.get(), &NotificationResponder::respond, Qt::QueuedConnection);
   responders_.push_back(responder);
}


NotificationTabResponder::NotificationTabResponder(const Ui::BSTerminalMainWindow *mainWinUi, QObject *parent)
   : NotificationResponder(parent), mainWinUi_(mainWinUi), iconDot_(QIcon(QLatin1String(":/ICON_DOT")))
{
   mainWinUi_->tabWidget->setIconSize(QSize(8, 8));
   connect(mainWinUi_->tabWidget, &QTabWidget::currentChanged, [this](int index) {
      mainWinUi_->tabWidget->setTabIcon(index, QIcon());
   });
}

void NotificationTabResponder::respond(bs::ui::NotifyType nt, bs::ui::NotifyMessage msg)
{
   const auto tabAction = getTabActionFor(nt, msg);
   if ((tabAction.index >= 0) && (mainWinUi_->tabWidget->currentIndex() != tabAction.index)) {
      mainWinUi_->tabWidget->setTabIcon(tabAction.index, tabAction.checked ? iconDot_ : QIcon());
   }
}

NotificationTabResponder::TabAction NotificationTabResponder::getTabActionFor(bs::ui::NotifyType nt, bs::ui::NotifyMessage msg) const
{
   switch (nt) {
   case bs::ui::NotifyType::DealerQuotes:
      if (msg.empty()) {
         return { -1, false };
      }
      return { mainWinUi_->tabWidget->indexOf(mainWinUi_->tabDealing), (msg[0].toInt() > 0) };

   case bs::ui::NotifyType::BlockchainTX:
      return { mainWinUi_->tabWidget->indexOf(mainWinUi_->tabTransactions), true };

   default: break;
   }
   return { -1, false };
}


NotificationTrayIconResponder::NotificationTrayIconResponder(const std::shared_ptr<QSystemTrayIcon> &trayIcon
   , const std::shared_ptr<ApplicationSettings> &appSettings, QObject *parent)
   : NotificationResponder(parent), trayIcon_(trayIcon), appSettings_(appSettings)
{
   connect(trayIcon_.get(), &QSystemTrayIcon::messageClicked, this, &NotificationTrayIconResponder::newVersionMessageClicked);
}

void NotificationTrayIconResponder::respond(bs::ui::NotifyType nt, bs::ui::NotifyMessage msg)
{
   QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;
   QString title, text;
   int msecs = 10000;
   newVersionMessage_ = false;

   switch (nt) {
   case bs::ui::NotifyType::BlockchainTX:
      if ((msg.size() < 2) || !appSettings_->get<bool>(ApplicationSettings::notifyOnTX)) {
         return;
      }
      title = msg[0].toString();
      text = msg[1].toString();
      break;

   case bs::ui::NotifyType::CelerOrder:
      if (msg.size() < 2) {
         return;
      }
      if (msg[0].toBool()) {
         title = tr("Order Filled");
         text = tr("Trade order %1 has been filled by the matching system").arg(msg[1].toString());
      }
      else {
         title = tr("Order Failed");
         text = tr("Order %1 was rejected with reason: %2").arg(msg[1].toString()).arg(msg[2].toString());
         icon = QSystemTrayIcon::Warning;
      }
      break;

   case bs::ui::NotifyType::AuthAddress:
      if (msg.size() < 2) {
         return;
      }
      title = tr("Auth address %1").arg(msg[1].toString());
      text = msg[0].toString();
      break;

   case bs::ui::NotifyType::BroadcastError:
      if (msg.size() < 2) {
         return;
      }
      title = tr("TX broadcast error");
      text = msg[1].toString();
      icon = QSystemTrayIcon::Critical;
      break;

   case bs::ui::NotifyType::NewVersion:
      if (msg.size() != 1) {
         return;
      }
      title = tr("New Terminal version: %1").arg(msg[0].toString());
      text = tr("Click this message to download it from BlockSettle's official site");
      msecs = 30000;
      newVersionMessage_ = true;
      break;

   default: return;
   }

   trayIcon_->showMessage(title, text, icon, msecs);
}

void NotificationTrayIconResponder::newVersionMessageClicked()
{
   if (newVersionMessage_) {
      const auto url = appSettings_->get<std::string>(ApplicationSettings::Binaries_Dl_Url);
      const auto title = tr("New version download");
      const auto errDownload = tr("Failed to open download URL");
#if defined (Q_OS_WIN)
      if ((unsigned int)ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL) <= 32) {
         MessageBoxWarning mb(title, errDownload);
         mb.exec();
      }
#elif defined (Q_OS_LINUX)
      char buf[256];
      snprintf(buf, sizeof(buf), "xdg-open '%s'", url.c_str());
      if (system(buf) != 0) {
         MessageBoxWarning mb(title, errDownload);
         mb.exec();
      }
#elif defined (Q_OS_OSX)
      char buf[256];
      snprintf(buf, sizeof(buf), "open '%s'", url.c_str());
      if (system(buf) != 0) {
         MessageBoxWarning mb(title, errDownload);
         mb.exec();
   }
#else
      MessageBoxWarning mb(title, tr("Shell execution is not supported on this platform, yet"));
      mb.exec();
#endif
   }
}
