#pragma once

#include <QString>
#include "Wallets/SignerDefs.h"

namespace gui_utils {
    QString satoshiToQString(int64_t balance);
    QString xbtToQString(double balance);
    QString directionToQString(bs::sync::Transaction::Direction direction);

    static const QString dateTimeFormat = QString::fromStdString("yyyy-MM-dd hh:mm:ss");
}
