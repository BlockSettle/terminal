#include "WalletKeysDeleteDevice.h"
#include "ui_WalletKeysDeleteDevice.h"

WalletKeysDeleteDevice::WalletKeysDeleteDevice(const QString &deviceName, QWidget *parent) :
   QWidget(parent),
   ui_(new Ui::WalletKeysDeleteDevice)
{
   ui_->setupUi(this);

   ui_->deviceDeleteLabel->setText(deviceName);

   connect(ui_->deviceDeleteButton, &QToolButton::clicked, this, &WalletKeysDeleteDevice::deleteClicked);
}

WalletKeysDeleteDevice::~WalletKeysDeleteDevice() = default;
