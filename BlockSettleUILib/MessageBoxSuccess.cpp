#include "MessageBoxSuccess.h"

#include "ui_MessageBoxSuccess.h"

MessageBoxSuccess::MessageBoxSuccess(const QString& title, const QString& text, QWidget* parent)
   : MessageBoxSuccess(title, text, QString(), parent)
{}

MessageBoxSuccess::MessageBoxSuccess(const QString& title, const QString& text
  , const QString& details, QWidget* parent)
   : CommonMessageBoxDialog(parent)
{
   ui_ = new Ui::MessageBoxSuccess();
   ui_->setupUi(this);

   setWindowTitle(title);
   ui_->labelText->setText(text.toUpper());

   if (details.isEmpty()) {
      ui_->pushButtonShowDetails->hide();
   } else {
      connect(ui_->pushButtonShowDetails, &QPushButton::clicked, this, &MessageBoxSuccess::OnDetailsPressed);
      ui_->labelDetails->setText(details);
   }

   HideDetails();

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &MessageBoxSuccess::accept);
}

void MessageBoxSuccess::OnDetailsPressed()
{
   if (AreDetailsVisible()) {
      ui_->pushButtonShowDetails->setText(tr("Show Details"));
      HideDetails();
   } else {
      ui_->pushButtonShowDetails->setText(tr("Hide Details"));
      ShowDetails();
   }
}

bool MessageBoxSuccess::AreDetailsVisible() const
{
   return ui_->verticalWidgetDetails->isVisible();
}

void MessageBoxSuccess::HideDetails()
{
   ui_->verticalWidgetDetails->hide();
   UpdateSize();
}

void MessageBoxSuccess::ShowDetails()
{
   ui_->verticalWidgetDetails->show();
   UpdateSize();
}


