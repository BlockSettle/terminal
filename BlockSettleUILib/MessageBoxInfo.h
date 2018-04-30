#ifndef __MESSAGE_BOX_INFO_H__
#define __MESSAGE_BOX_INFO_H__

#include "CommonMessageBoxDialog.h"

namespace Ui
{
   class MessageBoxInfo;
};

class MessageBoxInfo : public CommonMessageBoxDialog
{
Q_OBJECT

public:
   MessageBoxInfo(const QString& title, const QString& text, QWidget* parent = nullptr);
   MessageBoxInfo(const QString& title, const QString& text
      , const QString& details, QWidget* parent = nullptr);

   ~MessageBoxInfo() noexcept override = default;

private slots:
   void OnDetailsPressed();

private:
   bool AreDetailsVisible() const;

   void HideDetails();
   void ShowDetails();

private:
   Ui::MessageBoxInfo* ui_;
};


#endif // __MESSAGE_BOX_INFO_H__
