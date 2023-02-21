#pragma once

#include <QString>

namespace gui_utils {

QString satoshiToQString(int64_t balance);
QString normalizedSatoshiToQString(double balance);

}
