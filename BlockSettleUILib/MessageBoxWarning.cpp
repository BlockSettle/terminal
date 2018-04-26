#include "MessageBoxWarning.h"

#include "ui_MessageBoxWarning.h"

MessageBoxWarning::MessageBoxWarning(const QString& text, const QString& description, QWidget* parent)
   : MessageBoxWarning(text, description, QString(), parent)
{}

MessageBoxWarning::MessageBoxWarning(const QString& text, const QString& description
  , const QString& details, QWidget* parent)
   : CommonMessageBoxDialog(parent)
{
   ui_ = new Ui::MessageBoxWarning();
   ui_->setupUi(this);
   ui_->labelDetails->setTextFormat(Qt::RichText);
   ui_->labelDetails->setOpenExternalLinks(true);

   ui_->labelText->setText(text.toUpper());
   ui_->labelDescription->setText(description);
   if (details.isEmpty()) {
      ui_->pushButtonShowDetails->hide();
   } else {
      connect(ui_->pushButtonShowDetails, &QPushButton::clicked, this, &MessageBoxWarning::OnDetailsPressed);
      ui_->labelDetails->setText(details);
   }

   HideDetails();

   connect(ui_->pushButtonConfirm, &QPushButton::clicked, this, &MessageBoxWarning::accept);
}

void MessageBoxWarning::setButtonText(const QString &text)
{
   ui_->pushButtonConfirm->setText(text);
}

void MessageBoxWarning::OnDetailsPressed()
{
   if (AreDetailsVisible()) {
      ui_->pushButtonShowDetails->setText(tr("Show Details"));
      HideDetails();
   } else {
      ui_->pushButtonShowDetails->setText(tr("Hide Details"));
      ShowDetails();
   }
}

bool MessageBoxWarning::AreDetailsVisible() const
{
   return ui_->verticalWidgetDetails->isVisible();
}

void MessageBoxWarning::HideDetails()
{
   ui_->verticalWidgetDetails->hide();
   UpdateSize();
}

void MessageBoxWarning::ShowDetails()
{
   ui_->verticalWidgetDetails->show();
   UpdateSize();
}


