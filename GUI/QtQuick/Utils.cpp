#include "Utils.h"
#include "BTCNumericTypes.h"
#include <QObject>

using namespace gui_utils;

QString gui_utils::satoshiToQString(int64_t balance)
{
   return xbtToQString(balance / BTCNumericTypes::BalanceDivider);
}

QString gui_utils::xbtToQString(double balance)
{
   return QString::number(balance, 'f', 8);
}

QString gui_utils::directionToQString(bs::sync::Transaction::Direction direction)
{
   switch (direction) {
      case bs::sync::Transaction::Direction::Received:   return QObject::tr("Received");
      case bs::sync::Transaction::Direction::Sent:       return QObject::tr("Sent");
      case bs::sync::Transaction::Direction::Internal:   return QObject::tr("Internal");
      case bs::sync::Transaction::Direction::Unknown:    return QObject::tr("Unknown");
      default: return QString::number(static_cast<int>(direction));
   }
}
