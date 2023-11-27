/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "StartupDialog.h"

#include <QFile>
#include <QScreen>
#include <QStandardItemModel>

#include "ApplicationSettings.h"
#include "ui_StartupDialog.h"
#include "../../Core/ArmoryServersProvider.h"

namespace {
   const char *kLicenseFilePath = "://resources/license.html";
   const QString kLicenseAgreementTitle = QObject::tr("License Agreement");
   const QString kEnvConnectivityTitle = QObject::tr("BlockSettle Environment");

   const QString kOkButton = QObject::tr("Ok");
   const QString kCancelButton = QObject::tr("Cancel");
   const QString kAgreeButton = QObject::tr("Agree");
   const QString kDoneButton = QObject::tr("Continue");

   const QString kNetworkType = QObject::tr("ENVIRONMENT");
   const QString kProductionConnectivity = QObject::tr("Production Environment (Mainnet)");
   const QString kTestConnectivity = QObject::tr("Test Environment (Testnet)");
}

StartupDialog::StartupDialog(bool showLicense, QWidget *parent) :
  QDialog(parent)
  , ui_(new Ui::StartupDialog)
  , showLicense_(showLicense)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonBack, &QPushButton::clicked, this, &StartupDialog::onBack);
   connect(ui_->pushButtonNext, &QPushButton::clicked, this, &StartupDialog::onNext);
   ui_->stackedWidget->setCurrentIndex(showLicense_ ? Pages::LicenseAgreement : Pages::Settings);

   QFile file;
   file.setFileName(QLatin1String(kLicenseFilePath));
   file.open(QIODevice::ReadOnly);

   QString licenseText = QString::fromUtf8(file.readAll());

   ui_->textBrowserLicense->setHtml(licenseText);
   setupConnectivityList();
   updateStatus();
}

StartupDialog::~StartupDialog() = default;

void StartupDialog::init(const std::shared_ptr<ApplicationSettings> &appSettings)
{
   appSettings_ = appSettings;
   adjustPosition();
}

NetworkType StartupDialog::getSelectedNetworkType() const
{
   if (!ui_->envConnectivityListView->selectionModel()->hasSelection()) {
      return NetworkType::Invalid;
   }

   auto selectedItem = ui_->envConnectivityListView->selectionModel()->selectedRows()[0];
   const int selectedIndex = selectedItem.row();

   if (selectedIndex == 0) {
      return NetworkType::MainNet;
   }
   else {
      return NetworkType::TestNet;
   }
}

void StartupDialog::applySelectedConnectivity()
{
   NetworkType network = getSelectedNetworkType();

   ApplicationSettings::EnvConfiguration envConfig = (network == NetworkType::TestNet) ?
      ApplicationSettings::EnvConfiguration::Test : ApplicationSettings::EnvConfiguration::Production;
   appSettings_->set(ApplicationSettings::envConfiguration, static_cast<int>(envConfig));
   appSettings_->set(ApplicationSettings::initialized, true);
   appSettings_->set(ApplicationSettings::netType, static_cast<int>(network));

   appSettings_->selectNetwork();
}

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
#ifdef PRODUCTION_BUILD
   const bool showSettingsPage = false;
#else
   const bool showSettingsPage = true;
#endif
   if (!showLicense_ || ui_->stackedWidget->currentIndex() == Pages::Settings || !showSettingsPage) {
      accept();
      return;
   }

   ui_->stackedWidget->setCurrentIndex(Pages::Settings);
   updateStatus();
}

void StartupDialog::onConnectivitySelectionChanged()
{
   ui_->pushButtonNext->setDisabled(!ui_->envConnectivityListView->selectionModel()->hasSelection());
}

void StartupDialog::updateStatus()
{
   int currentPage = ui_->stackedWidget->currentIndex();

   ui_->pushButtonNext->setEnabled(true);
   if (currentPage == Pages::LicenseAgreement) {
      setWindowTitle(kLicenseAgreementTitle);
   } else {
      setWindowTitle(kEnvConnectivityTitle);
      onConnectivitySelectionChanged();

      adjustSize();
      adjustPosition();
   }

   if (!showLicense_) {
      ui_->pushButtonBack->setText(kCancelButton);
      ui_->pushButtonNext->setText(kDoneButton);
      return;
   }

   switch (currentPage)
   {
   case Pages::LicenseAgreement:
      ui_->pushButtonBack->setText(kCancelButton);
      ui_->pushButtonNext->setText(kAgreeButton);
      break;

   case Pages::Settings:
      ui_->pushButtonBack->hide();
      ui_->pushButtonNext->setText(kDoneButton);
      break;
   }
}

void StartupDialog::adjustPosition()
{
   auto currentDisplay = QGuiApplication::primaryScreen();
   auto rect = geometry();
   rect.moveCenter(currentDisplay->geometry().center());
   setGeometry(rect);
}

void StartupDialog::setupConnectivityList()
{
   ui_->envConnectivityListView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->envConnectivityListView->horizontalHeader()->setStretchLastSection(true);

   auto *connectivity = ui_->envConnectivityListView;
   auto *model = new QStandardItemModel(2, 1, connectivity);
   auto item0 = new QStandardItem(kProductionConnectivity);
   auto item1 = new QStandardItem(kTestConnectivity);
   item0->setEditable(false);
   item1->setEditable(false);
   model->setItem(0, item0);
   model->setItem(1, item1);
   model->setHorizontalHeaderItem(0, new QStandardItem(kNetworkType));
   connectivity->setModel(model);
   connectivity->selectRow(0);

   connect(ui_->envConnectivityListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &StartupDialog::onConnectivitySelectionChanged);
}
