#ifndef __MESSAGE_BOX_QUESTION_H__
#define __MESSAGE_BOX_QUESTION_H__

#include "CommonMessageBoxDialog.h"

namespace Ui
{
   class MessageBoxQuestion;
};

class MessageBoxQuestion : public CommonMessageBoxDialog
{
Q_OBJECT

public:
   MessageBoxQuestion(const QString& title, const QString& text, const QString& description, QWidget* parent = nullptr);
   MessageBoxQuestion(const QString& title, const QString& text, const QString& description
      , const QString& details, QWidget* parent = nullptr);

   ~MessageBoxQuestion() noexcept override = default;

   MessageBoxQuestion &setExclamationIcon();
   MessageBoxQuestion &setConfirmButtonText(const QString &);
   MessageBoxQuestion &setCancelButtonText(const QString &);

private slots:
   void OnDetailsPressed();

private:
   bool AreDetailsVisible() const;

   void HideDetails();
   void ShowDetails();

private:
   Ui::MessageBoxQuestion* ui_;
};


class MessageBoxCCWalletQuestion : public MessageBoxQuestion
{
public:
   MessageBoxCCWalletQuestion(const QString &ccProduct, QWidget *parent = nullptr);
};


#endif // __MESSAGE_BOX_QUESTION_H__
