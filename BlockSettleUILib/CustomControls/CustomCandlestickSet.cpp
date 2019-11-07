#include "CustomCandlestickSet.h"


CustomCandlestickSet::CustomCandlestickSet(qreal open, qreal high, qreal low, qreal close, qreal volume, qreal timestamp, QObject *parent)
   : QCandlestickSet(open, high, low, close, timestamp, parent)
   , volume_(volume) {

}
