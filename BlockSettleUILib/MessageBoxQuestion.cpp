#include "MessageBoxQuestion.h"

#include "ui_MessageBoxQuestion.h"

MessageBoxQuestion::MessageBoxQuestion(const QString& title, const QString& text, const QString& description, QWidget* parent)
   : MessageBoxQuestion(title, text, description, QString(), parent)
{}

MessageBoxQuestion::MessageBoxQuestion(const QString& title, const QString& text, const QString& description
  , const QString& details, QWidget* parent)
   : CommonMessageBoxDialog(parent)
{
   ui_ = new Ui::MessageBoxQuestion();
   ui_->setupUi(this);

   setWindowTitle(title);
   ui_->labelText->setText(text.toUpper());
   ui_->labelDescription->setText(description);

   if (details.isEmpty()) {
      ui_->pushButtonShowDetails->hide();
   } else {
      connect(ui_->pushButtonShowDetails, &QPushButton::clicked, this, &MessageBoxQuestion::OnDetailsPressed);
      ui_->labelDetails->setText(details);
   }

   HideDetails();

   connect(ui_->pushButtonConfirm, &QPushButton::clicked, this, &MessageBoxQuestion::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &MessageBoxQuestion::reject);
}

MessageBoxQuestion &MessageBoxQuestion::setExclamationIcon()
{
   ui_->labelIcon->setPixmap(QPixmap(QLatin1String(":/resources/wallet_warning.png")));
   ui_->labelText->setProperty("statusWarningLabel", false);
   ui_->labelText->setProperty("statusImportantLabel", true);
   ui_->labelText->style()->unpolish(ui_->labelText);
   ui_->labelText->style()->polish(ui_->labelText);
   return *this;
}

MessageBoxQuestion &MessageBoxQuestion::setConfirmButtonText(const QString &text)
{
   ui_->pushButtonConfirm->setText(text);
   return *this;
}

MessageBoxQuestion &MessageBoxQuestion::setCancelButtonText(const QString &text)
{
   ui_->pushButtonCancel->setText(text);
   return *this;
}

void MessageBoxQuestion::OnDetailsPressed()
{
   if (AreDetailsVisible()) {
      ui_->pushButtonShowDetails->setText(tr("Show Details"));
      HideDetails();
   } else {
      ui_->pushButtonShowDetails->setText(tr("Hide Details"));
      ShowDetails();
   }
}

bool MessageBoxQuestion::AreDetailsVisible() const
{
   return ui_->verticalWidgetDetails->isVisible();
}

void MessageBoxQuestion::HideDetails()
{
   ui_->verticalWidgetDetails->hide();
   UpdateSize();
}

void MessageBoxQuestion::ShowDetails()
{
   ui_->verticalWidgetDetails->show();
   UpdateSize();
}

MessageBoxCCWalletQuestion::MessageBoxCCWalletQuestion(const QString &ccProduct, QWidget *parent)
   : MessageBoxQuestion(tr("Private Market Wallet"), tr("CREATE %1 WALLET").arg(ccProduct)
      , tr("Your wallet does not have a branch in which to hold %1 tokens, would you like to create it?")
      .arg(ccProduct), parent)
{}
