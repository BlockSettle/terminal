/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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
#include "ArmoryServersProvider.h"

namespace {
   const char *kLicenseFilePath = "://resources/license.txt";
   const QString kLicenseAgreementTitle = QObject::tr("License Agreement");
   const QString kEnvConnectivityTitle = QObject::tr("BlockSettle Environment Connection");

   const QString kOkButton = QObject::tr("Ok");
   const QString kCancelButton = QObject::tr("Cancel");
   const QString kAgreeButton = QObject::tr("Agree");
   const QString kBackButton = QObject::tr("Back");
   const QString kDoneButton = QObject::tr("Done");

   const QString kNetworkType = QObject::tr("NETWORK TYPE");
   const QString kProductionConnectivity = QObject::tr("BlockSettle Production Environment");
   const QString kTestConnectivity = QObject::tr("BlockSettle Test Environment");
}

StartupDialog::StartupDialog(bool showLicense, QWidget *parent) :
  QDialog(parent)
  , ui_(new Ui::StartupDialog)
  , showLicense_(showLicense)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonBack, &QPushButton::clicked, this, &StartupDialog::onBack);
   connect(ui_->pushButtonNext, &QPushButton::clicked, this, &StartupDialog::onNext);
   connect(ui_->envConnectivityListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &StartupDialog::onConnectivitySelectionChanged);
   ui_->stackedWidget->setCurrentIndex(showLicense_ ? Pages::LicenseAgreement : Pages::Settings);

   QFile file;
   file.setFileName(QLatin1String(kLicenseFilePath));
   file.open(QIODevice::ReadOnly);

   QString licenseText = QString::fromUtf8(file.readAll());

   ui_->textBrowserLicense->setPlainText(licenseText);
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

void StartupDialog::applySelectedConnectivity(std::shared_ptr<ArmoryServersProvider> &armoryServersProvider)
{
   NetworkType network = getSelectedNetworkType();

   auto servers = armoryServersProvider->servers();
   int selectedIndex = 0;
   for (; selectedIndex < servers.size(); ++selectedIndex) {
      auto &server = servers[selectedIndex];
      if (server.name == QString::fromLatin1(ARMORY_BLOCKSETTLE_NAME) && server.netType == network) {
         break;
      }
   }
   assert(selectedIndex != servers.size());
   armoryServersProvider->setupServer(selectedIndex);

   ApplicationSettings::EnvConfiguration envConfig = (network == NetworkType::TestNet) ?
      ApplicationSettings::EnvConfiguration::Test : ApplicationSettings::EnvConfiguration::Production;
   appSettings_->set(ApplicationSettings::envConfiguration, static_cast<int>(envConfig));
   appSettings_->set(ApplicationSettings::initialized, true);
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
   if (!showLicense_ || ui_->stackedWidget->currentIndex() == Pages::Settings) {
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
      ui_->pushButtonBack->setText(kBackButton);
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
   model->setItem(0, new QStandardItem(kProductionConnectivity));
   model->setItem(1, new QStandardItem(kTestConnectivity));
   model->setHorizontalHeaderItem(0, new QStandardItem(kNetworkType));
   connectivity->setModel(model);
   connectivity->selectRow(1);
}
