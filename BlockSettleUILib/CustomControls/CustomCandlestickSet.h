/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CUSTOMCANDLESTICKSET_H
#define CUSTOMCANDLESTICKSET_H

#include <QCandlestickSet>

using namespace QtCharts;

class CustomCandlestickSet : public QCandlestickSet
{
    Q_OBJECT
public:
    CustomCandlestickSet(qreal open, qreal high, qreal low, qreal close, qreal volume, qreal timestamp = 0.0, QObject *parent = nullptr);
    qreal volume() { return volume_; }
    void setVolume(qreal volume) { volume_ = volume; }

signals:

public slots:

private:
   qreal volume_;
};

#endif // CUSTOMCANDLESTICKSET_H