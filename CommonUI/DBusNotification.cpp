/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#ifdef BS_USE_DBUS

#include "DBusNotification.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QStyle>
#include <QApplication>
#include <QDBusConnection>

// https://wiki.ubuntu.com/NotificationDevelopmentGuidelines recommends at least 128
const int FREEDESKTOP_NOTIFICATION_ICON_SIZE = 128;

static const QString c_defaultAction = QLatin1String("BlockSettleNotificationActionOk");


//
// DBusNotification
//

DBusNotification::DBusNotification(const QString &appName, QObject *parent)
   : QObject(parent)
   , appName_(appName)
   , interface_(new QDBusInterface(QLatin1String("org.freedesktop.Notifications"),
                                   QLatin1String("/org/freedesktop/Notifications"),
                                   QLatin1String("org.freedesktop.Notifications")))
{
   allowedActions_.insert(c_defaultAction);

   QDBusConnection::sessionBus().connect(QLatin1String("org.freedesktop.Notifications"),
      QLatin1String("/org/freedesktop/Notifications"),
      QLatin1String("org.freedesktop.Notifications"),
      QLatin1String("ActionInvoked"),
      QLatin1String("us"), this, SLOT(onAction(quint32, const QString&)));
}

bool DBusNotification::isValid() const
{
   return interface_->isValid();
}


//
// FreedesktopImage
//

// Loosely based on http://www.qtcentre.org/archive/index.php/t-25879.html
class FreedesktopImage
{
public:
    FreedesktopImage() {}
    explicit FreedesktopImage(const QImage &img);

    static int metaType();

    // Image to variant that can be marshalled over DBus
    static QVariant toVariant(const QImage &img);

private:
    int width, height, stride;
    bool hasAlpha;
    int channels;
    int bitsPerSample;
    QByteArray image;

    friend QDBusArgument &operator<<(QDBusArgument &a, const FreedesktopImage &i);
    friend const QDBusArgument &operator>>(const QDBusArgument &a, FreedesktopImage &i);
};

Q_DECLARE_METATYPE(FreedesktopImage);

// Image configuration settings
const int CHANNELS = 4;
const int BYTES_PER_PIXEL = 4;
const int BITS_PER_SAMPLE = 8;

FreedesktopImage::FreedesktopImage(const QImage &img):
    width(img.width()),
    height(img.height()),
    stride(img.width() * BYTES_PER_PIXEL),
    hasAlpha(true),
    channels(CHANNELS),
    bitsPerSample(BITS_PER_SAMPLE)
{
    // Convert 00xAARRGGBB to RGBA bytewise (endian-independent) format
    QImage tmp = img.convertToFormat(QImage::Format_ARGB32);
    const uint32_t *data = reinterpret_cast<const uint32_t*>(tmp.bits());

    unsigned int num_pixels = width * height;
    image.resize(num_pixels * BYTES_PER_PIXEL);

    for(unsigned int ptr = 0; ptr < num_pixels; ++ptr)
    {
        image[ptr*BYTES_PER_PIXEL+0] = data[ptr] >> 16; // R
        image[ptr*BYTES_PER_PIXEL+1] = data[ptr] >> 8;  // G
        image[ptr*BYTES_PER_PIXEL+2] = data[ptr];       // B
        image[ptr*BYTES_PER_PIXEL+3] = data[ptr] >> 24; // A
    }
}

QDBusArgument &operator<<(QDBusArgument &a, const FreedesktopImage &i)
{
    a.beginStructure();
    a << i.width << i.height << i.stride << i.hasAlpha << i.bitsPerSample << i.channels << i.image;
    a.endStructure();
    return a;
}

const QDBusArgument &operator>>(const QDBusArgument &a, FreedesktopImage &i)
{
    a.beginStructure();
    a >> i.width >> i.height >> i.stride >> i.hasAlpha >> i.bitsPerSample >> i.channels >> i.image;
    a.endStructure();
    return a;
}

int FreedesktopImage::metaType()
{
    return qDBusRegisterMetaType<FreedesktopImage>();
}

QVariant FreedesktopImage::toVariant(const QImage &img)
{
    FreedesktopImage fimg(img);
    return QVariant(FreedesktopImage::metaType(), &fimg);
}


//
// DBusNotification
//

void DBusNotification::notifyDBus(QSystemTrayIcon::MessageIcon cls, const QString &title,
   const QString &text, const QIcon &icon, int millisTimeout,
   const QString &action, const QString &label)
{
   // Arguments for DBus call:
   QList<QVariant> args;

   // Program Name:
   args.append(appName_);

   // Unique ID of this notification type:
   args.append(0U);

   // Application Icon, empty string
   args.append(QString());

   // Summary
   args.append(title);

   // Body
   args.append(text);

   // Actions (none, actions are deprecated)
   QStringList actions;
   if (!action.isEmpty()) {
      allowedActions_.insert(action);

      actions << action << label;
   } else {
      actions << c_defaultAction << tr("OK");
   }
   args.append(actions);

   // Hints
   QVariantMap hints;

   // If no icon specified, set icon based on class
   QIcon tmpicon;
   if(icon.isNull())
   {
      QStyle::StandardPixmap sicon = QStyle::SP_MessageBoxQuestion;
      switch(cls)
      {
         case QSystemTrayIcon::Information : sicon = QStyle::SP_MessageBoxInformation; break;
         case QSystemTrayIcon::Warning : sicon = QStyle::SP_MessageBoxWarning; break;
         case QSystemTrayIcon::Critical : sicon = QStyle::SP_MessageBoxCritical; break;
         default: break;
      }
      tmpicon = QApplication::style()->standardIcon(sicon);
   }
   else
   {
      tmpicon = icon;
   }
   hints[QLatin1String("icon_data")] = FreedesktopImage::toVariant(
      tmpicon.pixmap(FREEDESKTOP_NOTIFICATION_ICON_SIZE).toImage());
   args.append(hints);

   // Timeout (in msec)
   args.append(millisTimeout);

   // "Fire and forget"
   interface_->callWithArgumentList(QDBus::NoBlock, QLatin1String("Notify"), args);
}

void DBusNotification::onAction(quint32, const QString &action)
{
   if (allowedActions_.contains(action)) {
      emit messageClicked();
      emit actionInvoked(action);
   }
}

#endif // BS_USE_DBUS
