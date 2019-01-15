#include "StartupDialog.h"

#include <QFile>
#include "ApplicationSettings.h"
#include "ui_StartupDialog.h"


namespace {

const char *kLicenseFilePath = "://resources/license.txt";

}

StartupDialog::StartupDialog(bool showLicense, QWidget *parent) :
  QDialog(parent)
  , ui_(new Ui::StartupDialog)
  , showLicense_(showLicense)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonBack, &QPushButton::clicked, this, &StartupDialog::onBack);
   connect(ui_->pushButtonNext, &QPushButton::clicked, this, &StartupDialog::onNext);
   connect(ui_->radioButtonArmoryBlockSettle, &QPushButton::clicked, this, &StartupDialog::updateStatus);
   connect(ui_->radioButtonArmoryCustom, &QPushButton::clicked, this, &StartupDialog::updateStatus);
   connect(ui_->radioButtonArmoryLocal, &QPushButton::clicked, this, &StartupDialog::updateStatus);
   connect(ui_->checkBoxTestnet, &QCheckBox::clicked, this, &StartupDialog::updatePort);

   ui_->stackedWidget->setCurrentIndex(showLicense_ ? Pages::LicenseAgreement : Pages::Settings);

   QFile file;
   file.setFileName(QLatin1String(kLicenseFilePath));
   file.open(QIODevice::ReadOnly);

   QString licenseText = QString::fromUtf8(file.readAll());

   ui_->textBrowserLicense->setPlainText(licenseText);

   updatePort();
   updateStatus();
}

StartupDialog::~StartupDialog() = default;

void StartupDialog::onBack()
{
   if (!showLicense_ || ui_->stackedWidget->currentIndex() == Pages::LicenseAgreement) {
      reject();
      return;
   }

   ui_->stackedWidget->setCurrentIndex(Pages::LicenseAgreement);
   updateStatus();
}

void StartupDialog::onNext()
{
   if (!showLicense_ || ui_->stackedWidget->currentIndex()  == Pages::Settings) {
      accept();
      return;
   }

   ui_->stackedWidget->setCurrentIndex(Pages::Settings);
   updateStatus();
}

void StartupDialog::updateStatus()
{
   ui_->armoryGroupBox->setVisible(ui_->radioButtonArmoryCustom->isChecked());

   int currentPage = ui_->stackedWidget->currentIndex();

   if (currentPage == Pages::LicenseAgreement) {
      setWindowTitle(tr("License Agreement"));
   } else {
      setWindowTitle(tr("Bitcoin Network Connection"));
   }

   if (!showLicense_) {
      ui_->pushButtonBack->setText(tr("Cancel"));
      ui_->pushButtonNext->setText(tr("Done"));
      return;
   } else {
      switch (currentPage)
      {
         case Pages::LicenseAgreement:
            ui_->pushButtonBack->setText(tr("Cancel"));
            ui_->pushButtonNext->setText(tr("Agree"));
         break;

         case Pages::Settings:
            ui_->pushButtonBack->setText(tr("Back"));
            ui_->pushButtonNext->setText(tr("Done"));
         break;
      }
   }
}

StartupDialog::RunMode StartupDialog::runMode() const
{
   if (ui_->radioButtonArmoryBlockSettle->isChecked()) {
      return RunMode::BlocksettleSN;
   }
   else if (ui_->radioButtonArmoryLocal->isChecked()) {
      return RunMode::Local;
   }
   else if (ui_->radioButtonArmoryCustom->isChecked()) {
      return RunMode::Custom;
   }
}

QString StartupDialog::armoryDbIp() const
{
   return ui_->lineEditArmoryDBHost->text();
}

int StartupDialog::armoryDbPort() const
{
   return ui_->spinBoxArmoryDBPort->value();
}

NetworkType StartupDialog::networkType() const
{
   return ui_->checkBoxTestnet->isChecked() ? NetworkType::TestNet : NetworkType::MainNet;
}

void StartupDialog::updatePort()
{
   ui_->spinBoxArmoryDBPort->setValue(ApplicationSettings::GetDefaultArmoryLocalPort(networkType()));
}
