/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#ifndef DBUSNOTIFICATION_H_INCLUDED
#define DBUSNOTIFICATION_H_INCLUDED

#ifdef BS_USE_DBUS

#include <QObject>
#include <QSystemTrayIcon>
#include <QScopedPointer>
#include <QDBusInterface>
#include <QSet>


QT_BEGIN_NAMESPACE
class QDBusError;
class QDBusMessage;
QT_END_NAMESPACE


//
// DBusNotification
//

//! Notification with DBus.
class DBusNotification final : public QObject
{
   Q_OBJECT

signals:
   void messageClicked();
   void actionInvoked(const QString &action);

public:
   explicit DBusNotification(const QString &appName, QObject *parent = nullptr);
   ~DBusNotification() noexcept override = default;

   bool isValid() const;

public slots:
   void notifyDBus(QSystemTrayIcon::MessageIcon cls, const QString &title,
      const QString &text, const QIcon &icon, int millisTimeout,
      const QString &action = QString(), const QString &label = QString());

private slots:
   void onAction(quint32 id, const QString &action);

private:
   QString appName_;
   QScopedPointer<QDBusInterface> interface_;
   QSet<QString> allowedActions_;
}; // class DBusNotification

#endif // BS_USE_DBUS

#endif // DBUSNOTIFICATION_H_INCLUDED
