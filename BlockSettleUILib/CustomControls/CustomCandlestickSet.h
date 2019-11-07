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