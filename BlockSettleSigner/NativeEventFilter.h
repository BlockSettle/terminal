#ifndef NATIVEEVENTFILTER_H
#define NATIVEEVENTFILTER_H


#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QDebug>


class NativeEventFilter : public QAbstractNativeEventFilter
{
public:
    virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
};

#endif // NATIVEEVENTFILTER_H
