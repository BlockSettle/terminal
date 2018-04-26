#ifndef __MESSAGE_BOX_WARNING_H__
#define __MESSAGE_BOX_WARNING_H__

#include "CommonMessageBoxDialog.h"

namespace Ui
{
   class MessageBoxWarning;
};

class MessageBoxWarning : public CommonMessageBoxDialog
{
Q_OBJECT

public:
   MessageBoxWarning(const QString& text, const QString& description, QWidget* parent = nullptr);
   MessageBoxWarning(const QString& text, const QString& description
      , const QString& details, QWidget* parent = nullptr);

   ~MessageBoxWarning() noexcept override = default;

   void setButtonText(const QString &);

private slots:
   void OnDetailsPressed();

private:
   bool AreDetailsVisible() const;

   void HideDetails();
   void ShowDetails();

private:
   Ui::MessageBoxWarning* ui_;
};


#endif // __MESSAGE_BOX_WARNING_H__
