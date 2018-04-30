#ifndef __MESSAGE_BOX_CRITICAL_H__
#define __MESSAGE_BOX_CRITICAL_H__

#include "CommonMessageBoxDialog.h"

namespace Ui
{
   class MessageBoxCritical;
};

class MessageBoxCritical : public CommonMessageBoxDialog
{
Q_OBJECT

public:
   MessageBoxCritical(const QString& text, const QString& description, QWidget* parent = nullptr);
   MessageBoxCritical(const QString& text, const QString& description
      , const QString& details, QWidget* parent = nullptr);

   ~MessageBoxCritical() noexcept override = default;

private slots:
   void OnDetailsPressed();

private:
   bool AreDetailsVisible() const;

   void HideDetails();
   void ShowDetails();

private:
   Ui::MessageBoxCritical* ui_;
};


class MessageBoxBroadcastError : public MessageBoxCritical
{
   Q_OBJECT
public:
   MessageBoxBroadcastError(const QString &details, QWidget *parent = nullptr);
};


#endif // __MESSAGE_BOX_CRITICAL_H__
