/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AutoSignQuoteWidget.h"
#include "ui_AutoSignQuoteWidget.h"

#include "AutoSignQuoteProvider.h"
#include "BSErrorCodeStrings.h"
#include "BSMessageBox.h"

#include <QFileDialog>
#include <QFileInfo>

namespace {
   constexpr int kSelectAQFileItemIndex = 1;
}

AutoSignQuoteWidget::AutoSignQuoteWidget(QWidget *parent) :
   QWidget(parent),
   ui_(new Ui::AutoSignQuoteWidget)
{
   ui_->setupUi(this);

   connect(ui_->checkBoxAQ, &ToggleSwitch::clicked, this, &AutoSignQuoteWidget::onAutoQuoteToggled);
   connect(ui_->comboBoxAQScript, SIGNAL(activated(int)), this, SLOT(scriptChanged(int)));
   connect(ui_->checkBoxAutoSign, &ToggleSwitch::clicked, this, &AutoSignQuoteWidget::onAutoSignToggled);

   ui_->comboBoxAQScript->setFirstItemHidden(true);
}

void AutoSignQuoteWidget::init(const std::shared_ptr<AutoSignScriptProvider> &autoSignProvider)
{
   autoSignProvider_ = autoSignProvider;
   fillScriptHistory();
   onAutoSignReady();

   ui_->checkBoxAutoSign->setChecked(autoSignProvider_->autoSignState() == bs::error::ErrorCode::NoError);

   connect(autoSignProvider_.get(), &AutoSignScriptProvider::autoSignQuoteAvailabilityChanged
      , this, &AutoSignQuoteWidget::onAutoSignReady);
   connect(autoSignProvider_.get(), &AutoSignScriptProvider::autoSignStateChanged
      , this, &AutoSignQuoteWidget::onAutoSignStateChanged);
   connect(autoSignProvider_.get(), &AutoSignScriptProvider::scriptHistoryChanged
      , this, &AutoSignQuoteWidget::fillScriptHistory);
   connect(autoSignProvider_.get(), &AutoSignScriptProvider::scriptLoadedChanged
      , this, &AutoSignQuoteWidget::validateGUI);

   ui_->labelAutoSignWalletName->setText(autoSignProvider_->getAutoSignWalletName());
}

AutoSignQuoteWidget::~AutoSignQuoteWidget() = default;

void AutoSignQuoteWidget::onAutoQuoteToggled()
{
   bool isValidScript = (ui_->comboBoxAQScript->currentIndex() > kSelectAQFileItemIndex);
   if (ui_->checkBoxAQ->isChecked() && !isValidScript) {
      BSMessageBox question(BSMessageBox::question
         , tr("Try to enable scripting")
         , tr("Script is not specified. Do you want to select a script from file?"));
      const bool answerYes = (question.exec() == QDialog::Accepted);
      if (answerYes) {
         const auto scriptFileName = askForScript();
         if (!scriptFileName.isEmpty()) {
            autoSignProvider_->init(scriptFileName);
         }
      }
   }

   if (autoSignProvider_->isScriptLoaded()) {
      autoSignProvider_->setScriptLoaded(false);
   } else {
      autoSignProvider_->init(ui_->comboBoxAQScript->currentData().toString());
   }

   validateGUI();
}

void AutoSignQuoteWidget::onAutoSignStateChanged()
{
   ui_->checkBoxAutoSign->setChecked(autoSignProvider_->autoSignState() == bs::error::ErrorCode::NoError);
   if (autoSignProvider_->autoSignState() != bs::error::ErrorCode::NoError
       && autoSignProvider_->autoSignState() != bs::error::ErrorCode::AutoSignDisabled) {
      BSMessageBox(BSMessageBox::warning, tr("Auto Signing")
         , tr("Failed to enable Auto Signing")
         , bs::error::ErrorCodeToString(autoSignProvider_->autoSignState())).exec();
   }

   ui_->labelAutoSignWalletName->setText(autoSignProvider_->getAutoSignWalletName());
}

void AutoSignQuoteWidget::onAutoSignReady()
{
   ui_->labelAutoSignWalletName->setText(autoSignProvider_->getAutoSignWalletName());
   const bool enableWidget = autoSignProvider_->isReady();
   ui_->groupBoxAutoSign->setEnabled(enableWidget);
   ui_->checkBoxAutoSign->setEnabled(enableWidget);
   ui_->checkBoxAQ->setEnabled(enableWidget);
   validateGUI();
}

void AutoSignQuoteWidget::onUserConnected(bool autoSigning, bool autoQuoting)
{
   if (autoSigning) {
      ui_->checkBoxAutoSign->setChecked(true);
      onAutoSignToggled();
   }
   if (autoQuoting) {
      ui_->checkBoxAQ->setChecked(true);
      onAutoQuoteToggled();
   }
}

void AutoSignQuoteWidget::fillScriptHistory()
{
   ui_->comboBoxAQScript->clear();
   int curIndex = 0;
   ui_->comboBoxAQScript->addItem(tr("Select script..."));
   ui_->comboBoxAQScript->addItem(tr("Load new script"));
   const auto scripts = autoSignProvider_->getScripts();
   if (!scripts.isEmpty()) {
      const auto lastScript = autoSignProvider_->getLastScript();
      for (int i = 0; i < scripts.size(); i++) {
         QFileInfo fi(scripts[i]);
         ui_->comboBoxAQScript->addItem(fi.fileName(), scripts[i]);
         if (scripts[i] == lastScript) {
            curIndex = i + kSelectAQFileItemIndex + 1; // note the "Load" row in the head
         }
      }
   }
   ui_->comboBoxAQScript->setCurrentIndex(curIndex);
}

void AutoSignQuoteWidget::scriptChanged(int curIndex)
{
   if (curIndex < kSelectAQFileItemIndex) {
      return;
   }

   if (curIndex == kSelectAQFileItemIndex) {
      const auto scriptFileName = askForScript();

      if (scriptFileName.isEmpty()) {
         fillScriptHistory();
         return;
      }

      autoSignProvider_->init(scriptFileName);
   } else {
      if (autoSignProvider_->isScriptLoaded()) {
         autoSignProvider_->deinit();
      }
   }
}

void AutoSignQuoteWidget::onAutoSignToggled()
{
   if (ui_->checkBoxAutoSign->isChecked()) {
      autoSignProvider_->tryEnableAutoSign();
   } else {
      autoSignProvider_->disableAutoSign();
   }
   ui_->checkBoxAutoSign->setChecked(autoSignProvider_->autoSignState() == bs::error::ErrorCode::NoError);
}

void AutoSignQuoteWidget::validateGUI()
{
   ui_->checkBoxAQ->setChecked(autoSignProvider_->isScriptLoaded());
}

QString AutoSignQuoteWidget::askForScript()
{
   auto lastDir = autoSignProvider_->getLastDir();
   if (lastDir.isEmpty()) {
      lastDir = AutoSignScriptProvider::getDefaultScriptsDir();
   }

   auto path = QFileDialog::getOpenFileName(this, tr("Open script file")
      , lastDir, tr("QML files (*.qml)"));

   if (!path.isEmpty()) {
      autoSignProvider_->setLastDir(path);
   }

   return path;
}
