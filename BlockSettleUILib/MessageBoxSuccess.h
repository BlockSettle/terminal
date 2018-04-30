#ifndef __MESSAGE_BOX_SUCCESS_H__
#define __MESSAGE_BOX_SUCCESS_H__

#include "CommonMessageBoxDialog.h"

namespace Ui
{
   class MessageBoxSuccess;
};

class MessageBoxSuccess : public CommonMessageBoxDialog
{
Q_OBJECT

public:
   MessageBoxSuccess(const QString& title, const QString& text, QWidget* parent = nullptr);
   MessageBoxSuccess(const QString& title, const QString& text
      , const QString& details, QWidget* parent = nullptr);

   ~MessageBoxSuccess() noexcept override = default;

private slots:
   void OnDetailsPressed();

private:
   bool AreDetailsVisible() const;

   void HideDetails();
   void ShowDetails();

private:
   Ui::MessageBoxSuccess* ui_;
};


#endif // __MESSAGE_BOX_SUCCESS_H__
