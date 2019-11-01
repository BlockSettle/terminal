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
   ui_->labelExpanded->hide();

   connect(ui_->pushButtonBack, &QPushButton::clicked, this, &StartupDialog::onBack);
   connect(ui_->pushButtonNext, &QPushButton::clicked, this, &StartupDialog::onNext);
   ui_->stackedWidget->setCurrentIndex(showLicense_ ? Pages::LicenseAgreement : Pages::Settings);

   QFile file;
   file.setFileName(QLatin1String(kLicenseFilePath));
   file.open(QIODevice::ReadOnly);

   QString licenseText = QString::fromUtf8(file.readAll());

   ui_->textBrowserLicense->setPlainText(licenseText);
   updateStatus();
}

void StartupDialog::init(const std::shared_ptr<ApplicationSettings> &appSettings, const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider)
{
   appSettings_ = appSettings;
   armoryServersProvider_ = armoryServersProvider;
   armoryServersWidget_ = new ArmoryServersWidget(armoryServersProvider_, appSettings_, ui_->widgetManageArmory);
   armoryServersWidget_->adaptForStartupDialog();
   ui_->widgetManageArmory->layout()->addWidget(armoryServersWidget_);
   armoryServersWidget_->show();
   connect(ui_->pushButtonConfigure, &QPushButton::clicked, [this](){
      armoryServersWidget_->onExpandToggled();
      ui_->pushButtonConfigure->hide();
      ui_->labelExpanded->show();
      ui_->labelSimple->hide();
   });
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
   int currentPage = ui_->stackedWidget->currentIndex();

   if (currentPage == Pages::LicenseAgreement) {
      setWindowTitle(tr("License Agreement"));
      ui_->pushButtonConfigure->hide();
   } else {
      setWindowTitle(tr("Bitcoin Network Connection"));
      if (!armoryServersWidget_->isExpanded()) {
         ui_->pushButtonConfigure->show();
      }
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
