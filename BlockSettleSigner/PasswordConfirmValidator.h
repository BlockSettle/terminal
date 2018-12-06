#ifndef PasswordConfirmValidator_H
#define PasswordConfirmValidator_H

#include <QObject>
#include <QQuickItem>
#include <QString>
#include <QValidator>

class PasswordConfirmValidator : public QValidator
{
    Q_OBJECT

    Q_PROPERTY(QString compareTo READ getCompareTo WRITE setCompareTo)
    Q_PROPERTY(QString statusMsg READ getStatusMsg NOTIFY statusMsgChanged)
    Q_PROPERTY(QString name READ getName WRITE setName)

public:
    PasswordConfirmValidator(QObject *parent = nullptr);
    State validate(QString &input, int &) const override;

    QLocale locale() const;
    void setLocale(const QLocale & locale);

    QString getCompareTo() const;
    void setCompareTo(const QString& compareTo);

    QString getStatusMsg() const;
    void setStatusMsg(const QString &erros) const;

    QString getName() const;
    void setName(const QString &name);

signals:
    void statusMsgChanged(const QString& newErrors) const;

private:
    QString compareTo_;
    QString name_ = tr("Password");

    QString tooShortTmpl_ = QString::fromStdString("%1s ") + tr("must be minimum of six (6) characters.");
    QString dontMatchMsgTmpl_ = QString::fromStdString("%1s ") +  tr("do not match!");
    QString tooLongTmpl_ = tr("Confirmation") + QString::fromStdString(" %1 ") + tr("too long!");
    QString validTmpl_ = QString::fromStdString("%1s ") + tr("match!");
    mutable QString statusMsg_;
};

#endif // PasswordConfirmValidator_H
