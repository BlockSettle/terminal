#include "CreateOTCRequestWidget.h"

#include "ui_CreateOTCRequestWidget.h"

#include <QComboBox>
#include <QPushButton>

CreateOTCRequestWidget::CreateOTCRequestWidget(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::CreateOTCRequestWidget{}}
{
   ui_->setupUi(this);

   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &CreateOTCRequestWidget::OnBuyClicked);
   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &CreateOTCRequestWidget::OnSellClicked);

   ui_->comboBoxRange->addItem(tr("1-5"), static_cast<int>(bs::network::OTCRangeID::Type::Range1_5));
   ui_->comboBoxRange->addItem(tr("5-10"), static_cast<int>(bs::network::OTCRangeID::Type::Range5_10));
   ui_->comboBoxRange->addItem(tr("10-50"), static_cast<int>(bs::network::OTCRangeID::Type::Range10_50));
   ui_->comboBoxRange->addItem(tr("50-100"), static_cast<int>(bs::network::OTCRangeID::Type::Range50_100));
   ui_->comboBoxRange->addItem(tr("100-250"), static_cast<int>(bs::network::OTCRangeID::Type::Range100_250));
   ui_->comboBoxRange->addItem(tr("250+"), static_cast<int>(bs::network::OTCRangeID::Type::Range250plus));

   connect(ui_->comboBoxRange, SIGNAL(activated(int)), this, SLOT(OnRangeSelected(int)));
   connect(ui_->checkBoxSendAsOwn, &QCheckBox::stateChanged, this, &CreateOTCRequestWidget::onSendAsOwnChanged);

   connect(ui_->pushButtonSubmit, &QPushButton::pressed, this, &CreateOTCRequestWidget::RequestCreated);
}

CreateOTCRequestWidget::~CreateOTCRequestWidget() = default;

void CreateOTCRequestWidget::onSendAsOwnChanged()
{
   ui_->checkBoxGetReply->setEnabled(ui_->checkBoxSendAsOwn->isChecked());
   ui_->checkBoxGetReply->setChecked(ui_->checkBoxSendAsOwn->isChecked());
}

void CreateOTCRequestWidget::setSubmitButtonEnabled(bool enabled)
{
   ui_->pushButtonSubmit->setEnabled(enabled);
}

void CreateOTCRequestWidget::OnSellClicked()
{
   ui_->pushButtonBuy->setChecked(false);
   ui_->pushButtonSell->setChecked(true);

   RequestUpdated();
}

void CreateOTCRequestWidget::OnBuyClicked()
{
   ui_->pushButtonBuy->setChecked(true);
   ui_->pushButtonSell->setChecked(false);

   RequestUpdated();
}

void CreateOTCRequestWidget::RequestUpdated()
{}

void CreateOTCRequestWidget::OnRangeSelected(int index)
{
   RequestUpdated();
}

bs::network::ChatOTCSide::Type CreateOTCRequestWidget::GetSide() const
{
   if (ui_->pushButtonSell->isChecked()) {
      return bs::network::ChatOTCSide::Sell;
   }

   return bs::network::ChatOTCSide::Buy;
}

bs::network::OTCRangeID::Type CreateOTCRequestWidget::GetRange() const
{
   return static_cast<bs::network::OTCRangeID::Type>(ui_->comboBoxRange->itemData(ui_->comboBoxRange->currentIndex()).toInt());
}

bool CreateOTCRequestWidget::SendAsOwn() const
{
   return ui_->checkBoxSendAsOwn->isChecked();
}

bool CreateOTCRequestWidget::ReplyRequired() const
{
   return ui_->checkBoxGetReply->isChecked();
}
