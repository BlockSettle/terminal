#include "CreateOTCRequestWidget.h"

#include "OtcTypes.h"
#include "ui_CreateOTCRequestWidget.h"

#include <QComboBox>
#include <QPushButton>

using namespace bs::network;

CreateOTCRequestWidget::CreateOTCRequestWidget(QWidget* parent)
   : OTCWindowsAdapterBase{parent}
   , ui_{new Ui::CreateOTCRequestWidget{}}
{
   ui_->setupUi(this);
}

CreateOTCRequestWidget::~CreateOTCRequestWidget() = default;

void CreateOTCRequestWidget::init(otc::Env env)
{
   for (int i = int(otc::firstRangeValue(env)); i <= int(otc::lastRangeValue(env)); ++i) {
      ui_->comboBoxRange->addItem(QString::fromStdString(otc::toString(otc::RangeType(i))), i);
   }

   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &CreateOTCRequestWidget::onBuyClicked);
   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &CreateOTCRequestWidget::onSellClicked);
   connect(ui_->pushButtonSubmit, &QPushButton::clicked, this, &CreateOTCRequestWidget::requestCreated);

   onSellClicked();
}

otc::QuoteRequest CreateOTCRequestWidget::request() const
{
   bs::network::otc::QuoteRequest result;
   result.rangeType = otc::RangeType(ui_->comboBoxRange->currentData().toInt());
   result.ourSide = ui_->pushButtonSell->isChecked() ? otc::Side::Sell : otc::Side::Buy;
   return result;
}

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
