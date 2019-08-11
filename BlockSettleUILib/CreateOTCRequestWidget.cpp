#include "CreateOTCRequestWidget.h"

#include "OtcTypes.h"
#include "ui_CreateOTCRequestWidget.h"

#include <QComboBox>
#include <QPushButton>

CreateOTCRequestWidget::CreateOTCRequestWidget(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::CreateOTCRequestWidget{}}
{
   ui_->setupUi(this);

   for (int i = 0; i < int(bs::network::otc::RangeType::Count); ++i) {
      auto range = bs::network::otc::RangeType(i);
      ui_->comboBoxRange->addItem(QString::fromStdString(bs::network::otc::toString(range)), i);
   }

   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &CreateOTCRequestWidget::onBuyClicked);
   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &CreateOTCRequestWidget::onSellClicked);

   onSellClicked();
}

CreateOTCRequestWidget::~CreateOTCRequestWidget() = default;

void CreateOTCRequestWidget::onSellClicked()
{
   ui_->pushButtonSell->setChecked(true);
   ui_->pushButtonBuy->setChecked(false);
}

void CreateOTCRequestWidget::onBuyClicked()
{
   ui_->pushButtonSell->setChecked(false);
   ui_->pushButtonBuy->setChecked(true);
}
