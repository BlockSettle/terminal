#include "MessageBoxCritical.h"

#include "ui_MessageBoxCritical.h"

MessageBoxCritical::MessageBoxCritical(const QString& text, const QString& description, QWidget* parent)
   : MessageBoxCritical(text, description, QString(), parent)
{}

MessageBoxCritical::MessageBoxCritical(const QString& text, const QString& description
  , const QString& details, QWidget* parent)
   : CommonMessageBoxDialog(parent)
{
   ui_ = new Ui::MessageBoxCritical();
   ui_->setupUi(this);

   ui_->labelText->setText(text.toUpper());
   ui_->labelDescription->setText(description);
   if (details.isEmpty()) {
      ui_->pushButtonShowDetails->hide();
   } else {
      connect(ui_->pushButtonShowDetails, &QPushButton::clicked, this, &MessageBoxCritical::OnDetailsPressed);
      ui_->labelDetails->setText(details);
   }

   HideDetails();

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &MessageBoxCritical::accept);
}

void MessageBoxCritical::OnDetailsPressed()
{
   if (AreDetailsVisible()) {
      ui_->pushButtonShowDetails->setText(tr("Show Details"));
      HideDetails();
   } else {
      ui_->pushButtonShowDetails->setText(tr("Hide Details"));
      ShowDetails();
   }
}

bool MessageBoxCritical::AreDetailsVisible() const
{
   return ui_->verticalWidgetDetails->isVisible();
}

void MessageBoxCritical::HideDetails()
{
   ui_->verticalWidgetDetails->hide();
   UpdateSize();
}

void MessageBoxCritical::ShowDetails()
{
   ui_->verticalWidgetDetails->show();
   UpdateSize();
}


MessageBoxBroadcastError::MessageBoxBroadcastError(const QString &details, QWidget *parent)
   : MessageBoxCritical(tr("FAILED TO SIGN TRANSACTION"), tr("Error occured when signing a transaction")
      , details,  parent)
{
   setWindowTitle(tr("Broadcast Failure"));
}
