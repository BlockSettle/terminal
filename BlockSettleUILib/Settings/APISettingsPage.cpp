/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "APISettingsPage.h"
#include "ui_APISettingsPage.h"
#include <QFileDialog>
#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "AutoSignQuoteProvider.h"
#include "UserScript.h"


APISettingsPage::APISettingsPage(QWidget* parent)
   : SettingsPage{parent}
   , ui_{new Ui::APISettingsPage{}}
{
   ui_->setupUi(this);

   connect(ui_->pushButtonSelectRFQ, &QPushButton::clicked, this
      , &APISettingsPage::onRFQSelect);

   auto logger = spdlog::get("");
   rfqLoader_ = new AutoRFQ(logger, nullptr, this);
}

APISettingsPage::~APISettingsPage() = default;

void APISettingsPage::display()
{
   ui_->toggleAutoStartRFQ->setChecked(appSettings_->get<bool>(ApplicationSettings::AutoStartRFQScript));

   rfqScriptFN_ = appSettings_->get<QString>(ApplicationSettings::CurrentRFQScript);
   displayRFQScriptFN();
}

void APISettingsPage::displayRFQScriptFN()
{
   if (rfqScriptFN_.isEmpty()) {
      ui_->labelScriptFile->setText(tr("Select script"));
   } else {
      ui_->labelScriptFile->setText(rfqScriptFN_);
   }
}

void APISettingsPage::reset()
{
   appSettings_->reset(ApplicationSettings::AutoStartRFQScript);
   appSettings_->reset(ApplicationSettings::CurrentRFQScript);
   display();
}

void APISettingsPage::apply()
{
   appSettings_->set(ApplicationSettings::AutoStartRFQScript, ui_->toggleAutoStartRFQ->isChecked());
   appSettings_->set(ApplicationSettings::CurrentRFQScript, rfqScriptFN_);
}

void APISettingsPage::onRFQSelect()
{
   ui_->labelScriptFile->setText(tr("Script selection..."));
   auto lastDir = appSettings_->get<QString>(ApplicationSettings::LastAqDir);
   if (lastDir.isEmpty()) {
      lastDir = AutoSignScriptProvider::getDefaultScriptsDir();
   }
   const auto &path = QFileDialog::getOpenFileName(this, tr("Open script file")
      , lastDir, tr("QML files (*.qml)"));

   if (path.isEmpty()) {
      displayRFQScriptFN();
      return;
   }
   ui_->labelScriptFile->setText(tr("Loading %1...").arg(path));
   rfqScriptFN_ = path;
   
   if (rfqLoader_->load(path)) {
      ui_->labelScriptFile->setText(rfqScriptFN_);
   }
   else {
      ui_->labelScriptFile->setText(tr("Failed to load %1").arg(path));
      rfqScriptFN_.clear();
   }
}
