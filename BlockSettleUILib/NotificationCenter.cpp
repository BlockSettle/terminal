/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "NotificationCenter.h"
#include "ui_BSTerminalMainWindow.h"
#include "ApplicationSettings.h"
#include "BSMessageBox.h"
#include "spdlog/spdlog.h"

#if defined (Q_OS_WIN)
#include <shellapi.h>
#else
#include <stdlib.h>
#endif

static std::shared_ptr<NotificationCenter> globalInstance = nullptr;

NotificationCenter::NotificationCenter(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const Ui::BSTerminalMainWindow *mainWinUi
   , const std::shared_ptr<QSystemTrayIcon> &trayIcon, QObject *parent)
   : QObject(parent)
{
   qRegisterMetaType<bs::ui::NotifyType>("NotifyType");
   qRegisterMetaType<bs::ui::NotifyMessage>("NotifyMessage");

   addResponder(std::make_shared<NotificationTabResponder>(mainWinUi, appSettings, this));
   addResponder(std::make_shared<NotificationTrayIconResponder>(logger, mainWinUi, trayIcon, appSettings, this));
}

NotificationCenter::NotificationCenter(const std::shared_ptr<spdlog::logger>& logger
   , const Ui::BSTerminalMainWindow* mainWinUi
   , const std::shared_ptr<QSystemTrayIcon>& trayIcon, QObject* parent)
   : QObject(parent)
{
   qRegisterMetaType<bs::ui::NotifyType>("NotifyType");
   qRegisterMetaType<bs::ui::NotifyMessage>("NotifyMessage");

   addResponder(std::make_shared<NotificationTabResponder>(mainWinUi, this));
   addResponder(std::make_shared<NotificationTrayIconResponder>(logger, mainWinUi, trayIcon, this));
}

void NotificationCenter::createInstance(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<ApplicationSettings> &appSettings
   , const Ui::BSTerminalMainWindow *ui, const std::shared_ptr<QSystemTrayIcon> &trayIcon, QObject *parent)
{
   globalInstance = std::make_shared<NotificationCenter>(logger, appSettings, ui, trayIcon, parent);
}

NotificationCenter *NotificationCenter::instance()
{
   return globalInstance.get();
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


NotificationTabResponder::NotificationTabResponder(const Ui::BSTerminalMainWindow *mainWinUi,
   std::shared_ptr<ApplicationSettings> appSettings, QObject *parent)
   : NotificationResponder(parent), mainWinUi_(mainWinUi), iconDot_(QIcon(QLatin1String(":/ICON_DOT")))
   , appSettings_(appSettings)
{
   mainWinUi_->tabWidget->setIconSize(QSize(8, 8));
   connect(mainWinUi_->tabWidget, &QTabWidget::currentChanged, [this](int index) {
      mainWinUi_->tabWidget->setTabIcon(index, QIcon());
   });
}

NotificationTabResponder::NotificationTabResponder(const Ui::BSTerminalMainWindow* mainWinUi
   , QObject* parent)
   : NotificationResponder(parent), mainWinUi_(mainWinUi), iconDot_(QIcon(QLatin1String(":/ICON_DOT")))
{
   mainWinUi_->tabWidget->setIconSize(QSize(8, 8));
   connect(mainWinUi_->tabWidget, &QTabWidget::currentChanged, [this](int index) {
      mainWinUi_->tabWidget->setTabIcon(index, QIcon());
   });
}

void NotificationTabResponder::respond(bs::ui::NotifyType nt, bs::ui::NotifyMessage msg)
{
   if (nt == bs::ui::NotifyType::UpdateUnreadMessage) {
      return;
   }

   const auto tabAction = getTabActionFor(nt, msg);
   if ((tabAction.index >= 0) && (mainWinUi_->tabWidget->currentIndex() != tabAction.index)) {
      mainWinUi_->tabWidget->setTabIcon(tabAction.index,
         tabAction.checked && tabAction.enabled ? iconDot_ : QIcon());
   }
}

NotificationTabResponder::TabAction NotificationTabResponder::getTabActionFor(bs::ui::NotifyType nt, bs::ui::NotifyMessage msg) const
{
   switch (nt) {
   case bs::ui::NotifyType::DealerQuotes:
      if (msg.empty()) {
         return { -1, false, false };
      }
      return { mainWinUi_->tabWidget->indexOf(mainWinUi_->widgetRFQReply), (msg[0].toInt() > 0),
         appSettings_ ? !appSettings_->get<bool>(ApplicationSettings::DisableBlueDotOnTabOfRfqBlotter) : true};

   case bs::ui::NotifyType::BlockchainTX:
      return { mainWinUi_->tabWidget->indexOf(mainWinUi_->widgetTransactions), true, true };

   default: break;
   }
   return { -1, false, false };
}


NotificationTrayIconResponder::NotificationTrayIconResponder(const std::shared_ptr<spdlog::logger> &logger
   , const Ui::BSTerminalMainWindow *mainWinUi
   , const std::shared_ptr<QSystemTrayIcon> &trayIcon
   , const std::shared_ptr<ApplicationSettings> &appSettings, QObject *parent)
   : NotificationResponder(parent)
   , logger_(logger)
   , mainWinUi_(mainWinUi)
   , trayIcon_(trayIcon)
   , appSettings_(appSettings)
   , notifMode_(QSystemTray)
#ifdef BS_USE_DBUS
   , dbus_(new DBusNotification(tr("BlockSettle Terminal"), this))
#endif
{
   connect(trayIcon_.get(), &QSystemTrayIcon::messageClicked, this, &NotificationTrayIconResponder::messageClicked);

#ifdef BS_USE_DBUS
   if(dbus_->isValid()) {
      notifMode_ = Freedesktop;

      disconnect(trayIcon_.get(), &QSystemTrayIcon::messageClicked,
         this, &NotificationTrayIconResponder::messageClicked);
      connect(dbus_, &DBusNotification::actionInvoked,
         this, &NotificationTrayIconResponder::notificationAction);
   }
#endif // BS_USE_DBUS
}

NotificationTrayIconResponder::NotificationTrayIconResponder(const std::shared_ptr<spdlog::logger>& logger
   , const Ui::BSTerminalMainWindow* mainWinUi
   , const std::shared_ptr<QSystemTrayIcon>& trayIcon
   , QObject* parent)
   : NotificationResponder(parent)
   , logger_(logger)
   , mainWinUi_(mainWinUi)
   , trayIcon_(trayIcon)
   , notifMode_(QSystemTray)
#ifdef BS_USE_DBUS
   , dbus_(new DBusNotification(tr("BlockSettle Terminal"), this))
#endif
{
   connect(trayIcon_.get(), &QSystemTrayIcon::messageClicked, this, &NotificationTrayIconResponder::messageClicked);

#ifdef BS_USE_DBUS
   if (dbus_->isValid()) {
      notifMode_ = Freedesktop;

      disconnect(trayIcon_.get(), &QSystemTrayIcon::messageClicked,
         this, &NotificationTrayIconResponder::messageClicked);
      connect(dbus_, &DBusNotification::actionInvoked,
         this, &NotificationTrayIconResponder::notificationAction);
   }
#endif // BS_USE_DBUS
}

static const QString c_newVersionAction = QLatin1String("BlockSettleNewVersionAction");
static const QString c_newOkAction = QLatin1String("BlockSettleNotificationActionOk");

void NotificationTrayIconResponder::respond(bs::ui::NotifyType nt, bs::ui::NotifyMessage msg)
{
   QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;
   QString title, text, userId;
   int msecs = 10000;
   newVersionMessage_ = false;
   newChatMessage_ = false;
   newChatId_ = QString();

   const int chatIndex = mainWinUi_->tabWidget->indexOf(mainWinUi_->widgetChat);
   const bool isChatTabActive = mainWinUi_->tabWidget->currentIndex() == chatIndex && QApplication::activeWindow();
   auto updateChatIconAndCheckChatTab = [&]() -> bool {
      if (isChatTabActive) {
         mainWinUi_->tabWidget->setTabIcon(chatIndex, QIcon());
      } else {
         mainWinUi_->tabWidget->setTabIcon(chatIndex, QIcon(QLatin1String(":/ICON_DOT")));
      }
      return isChatTabActive;
   };

   switch (nt) {
   case bs::ui::NotifyType::BlockchainTX:
      if ((msg.size() < 2) || appSettings_ ? !appSettings_->get<bool>(ApplicationSettings::notifyOnTX) : false) {
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
      title = tr("Authentication Address %1").arg(msg[1].toString());
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

   case bs::ui::NotifyType::UpdateUnreadMessage: {
      if (msg.size() != 4) {
         return;
      }

      const bool forceNotification = msg[3].toBool();
      if (updateChatIconAndCheckChatTab() && !forceNotification) {
         return;
      }

      title = msg[0].toString();
      text = msg[1].toString();
      userId = msg[2].toString();

      if (title.isEmpty() || text.isEmpty() || userId.isEmpty()) {
         return;
      }

      newChatMessage_ = true;
      newChatId_ = userId;
      break;
   }
   case bs::ui::NotifyType::FriendRequest:
      if (updateChatIconAndCheckChatTab() || msg.size() != 1) {
         return;
      }

      title = tr("New contact request");
      text = tr("%1 would like to add you as a contact").arg(msg[0].toString());
      break;

   case bs::ui::NotifyType::OTCOrderError:
      if (msg.size() != 1) {
         return;
      }

      title = tr("OTC Failed");
      text = msg[0].toString();
      icon = QSystemTrayIcon::Warning;
      break;

   case bs::ui::NotifyType::BitcoinCoreOnline:
      title = tr("Bitcoin Core");
      text = tr("Connected to Bitcoin Core");
      icon = QSystemTrayIcon::Information;
      break;

   case bs::ui::NotifyType::BitcoinCoreOffline:
      title = tr("Bitcoin Core");
      text = tr("Bitcoin Core is offline");
      icon = QSystemTrayIcon::Critical;
      break;

   case bs::ui::NotifyType::LogOut:
      // hide icons in all tabs on user logout
      for (int i=0; i<mainWinUi_->tabWidget->count(); i++) {
         mainWinUi_->tabWidget->setTabIcon(i, QIcon());
      }
      return;

   case bs::ui::NotifyType::AccountDisabled:
      title = tr("Account Unavailable");
      text = tr("Your account is temporarily unavailable. We are working on having this solved as soon as possible. "
                "We apologise for the inconvenience, but we hope you bear with us as we endeavour to resolve the issue. "
                "If you have any questions, don't hesitate to contact our Support team through the Client Portal.");
      icon = QSystemTrayIcon::Critical;
      break;

   case bs::ui::NotifyType::AccountEnabled:
      title = tr("Account Activated");
      text = tr("Your account has now been activated again. We thank you for your patience and apologise for any inconvenience. "
                "If you have any questions, don't hesitate to contact our Support team through the Client Portal.");
      icon = QSystemTrayIcon::Information;
      break;
   case bs::ui::NotifyType::TradingEnabledOnPB:
      icon = QSystemTrayIcon::Information;
      title = tr("System status");
      text = tr("Order entry has resumed");
      break;
   case bs::ui::NotifyType::TradingDisabledOnPB:
      icon = QSystemTrayIcon::Information;
      title = tr("System status");
      text = tr("Order entry has been temporarily suspended");
      break;

   default: return;
   }

   SPDLOG_LOGGER_INFO(logger_, "notification: {} ({}) {}", title.toStdString(), text.toStdString(), userId.toStdString());

   if (notifMode_ == QSystemTray) {
      //trayIcon_->showMessage(title, text, icon, msecs);
      trayIcon_->showMessage(title, text, QIcon(QLatin1String(":/resources/login-logo.png")), msecs);
   }
#ifdef BS_USE_DBUS
   else {
      dbus_->notifyDBus(icon, title, text, QIcon(), msecs,
         (newVersionMessage_ ? c_newVersionAction : QString()),
         (newVersionMessage_ ? tr("Update") : QString()));
   }
#endif // BS_USE_DBUS
}

void NotificationTrayIconResponder::messageClicked()
{
   if (newVersionMessage_ && appSettings_) {
      const auto url = appSettings_->get<std::string>(ApplicationSettings::Binaries_Dl_Url);
      const auto title = tr("New version download");
      const auto errDownload = tr("Failed to open download URL");
#if defined (Q_OS_WIN)
      if ((unsigned int)ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL) <= 32) {
         BSMessageBox mb(BSMessageBox::warning, title, errDownload);
         mb.exec();
      }
#elif defined (Q_OS_LINUX)
      char buf[256];
      snprintf(buf, sizeof(buf), "xdg-open '%s'", url.c_str());
      if (system(buf) != 0) {
         BSMessageBox mb(BSMessageBox::warning, title, errDownload);
         mb.exec();
      }
#elif defined (Q_OS_OSX)
      char buf[256];
      snprintf(buf, sizeof(buf), "open '%s'", url.c_str());
      if (system(buf) != 0) {
         BSMessageBox mb(BSMessageBox::warning, title, errDownload);
         mb.exec();
   }
#else
      BSMessageBox mb(BSMessageBox::warning, title, tr("Shell execution is not supported on this platform, yet"));
      mb.exec();
#endif
   }
   else if (newChatMessage_) {
      if (!newChatId_.isNull() && globalInstance != nullptr) {
         const int chatIndex = mainWinUi_->tabWidget->indexOf(mainWinUi_->widgetChat);
         mainWinUi_->tabWidget->setTabIcon(chatIndex, QIcon());
         mainWinUi_->tabWidget->setCurrentWidget(mainWinUi_->widgetChat);
         auto window = mainWinUi_->tabWidget->window();
         if (window) {
            QMetaObject::invokeMethod(window, "raiseWindow", Qt::DirectConnection);
         }
         auto signal = QMetaMethod::fromSignal(&NotificationCenter::newChatMessageClick);
         signal.invoke(globalInstance.get(), Q_ARG(QString, newChatId_));
      }
   }
}

#ifdef BS_USE_DBUS
void NotificationTrayIconResponder::notificationAction(const QString &action)
{
   if (action == c_newVersionAction) {
      newVersionMessage_ = true;
      messageClicked();
   } else if (action == c_newOkAction) {
      newChatMessage_ = true;
      messageClicked();
   }
}
#endif // BS_USE_DBUS
