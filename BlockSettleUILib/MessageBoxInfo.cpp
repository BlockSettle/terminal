#include "MessageBoxInfo.h"

#include "ui_MessageBoxInfo.h"

MessageBoxInfo::MessageBoxInfo(const QString& title, const QString& text, QWidget* parent)
   : MessageBoxInfo(title, text, QString(), parent)
{}

MessageBoxInfo::MessageBoxInfo(const QString& title, const QString& text
  , const QString& details, QWidget* parent)
   : CommonMessageBoxDialog(parent)
{
   ui_ = new Ui::MessageBoxInfo();
   ui_->setupUi(this);

   setWindowTitle(title);
   ui_->labelText->setText(text);

   if (details.isEmpty()) {
      ui_->pushButtonShowDetails->hide();
   } else {
      connect(ui_->pushButtonShowDetails, &QPushButton::clicked, this, &MessageBoxInfo::OnDetailsPressed);
      ui_->labelDetails->setText(details);
   }

   HideDetails();

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &MessageBoxInfo::accept);
}

void MessageBoxInfo::OnDetailsPressed()
{
   if (AreDetailsVisible()) {
      ui_->pushButtonShowDetails->setText(tr("Show Details"));
      HideDetails();
   } else {
      ui_->pushButtonShowDetails->setText(tr("Hide Details"));
      ShowDetails();
   }
}

bool MessageBoxInfo::AreDetailsVisible() const
{
   return ui_->verticalWidgetDetails->isVisible();
}

void MessageBoxInfo::HideDetails()
{
   ui_->verticalWidgetDetails->hide();
   UpdateSize();
}

void MessageBoxInfo::ShowDetails()
{
   ui_->verticalWidgetDetails->show();
   UpdateSize();
}


