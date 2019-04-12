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

   ui_->comboBoxRange->addItem(tr("1-5"), bs::network::OTCRangeID::Range1_5);
   ui_->comboBoxRange->addItem(tr("5-10"), bs::network::OTCRangeID::Range5_10);
   ui_->comboBoxRange->addItem(tr("10-50"), bs::network::OTCRangeID::Range10_50);
   ui_->comboBoxRange->addItem(tr("50-100"), bs::network::OTCRangeID::Range50_100);
   ui_->comboBoxRange->addItem(tr("100-250"), bs::network::OTCRangeID::Range100_250);
   ui_->comboBoxRange->addItem(tr("250+"), bs::network::OTCRangeID::Range250plus);

   connect(ui_->comboBoxRange, SIGNAL(activated(int)), this, SLOT(OnRangeSelected(int)));

   connect(ui_->pushButtonSubmit, &QPushButton::pressed, this, &CreateOTCRequestWidget::RequestCreated);
}

CreateOTCRequestWidget::~CreateOTCRequestWidget() = default;

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

bs::network::Side::Type CreateOTCRequestWidget::GetSide() const
{
   if (ui_->pushButtonSell->isChecked()) {
      return bs::network::Side::Sell;
   }

   return bs::network::Side::Buy;
}

bs::network::OTCRangeID CreateOTCRequestWidget::GetRange() const
{
   return static_cast<bs::network::OTCRangeID>(ui_->comboBoxRange->itemData(ui_->comboBoxRange->currentIndex()).toInt());
}
