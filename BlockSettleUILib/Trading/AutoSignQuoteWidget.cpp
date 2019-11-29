/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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
   connect(ui_->comboBoxAQScript, SIGNAL(activated(int)), this, SLOT(aqScriptChanged(int)));
   connect(ui_->checkBoxAutoSign, &ToggleSwitch::clicked, this, &AutoSignQuoteWidget::onAutoSignToggled);

   ui_->comboBoxAQScript->setFirstItemHidden(true);
}

void AutoSignQuoteWidget::init(const std::shared_ptr<AutoSignQuoteProvider> &autoSignQuoteProvider)
{
   autoSignQuoteProvider_ = autoSignQuoteProvider;
   aqFillHistory();
   onAutoSignQuoteAvailChanged();

   connect(autoSignQuoteProvider_.get(), &AutoSignQuoteProvider::autoSignQuoteAvailabilityChanged, this, &AutoSignQuoteWidget::onAutoSignQuoteAvailChanged);
   connect(autoSignQuoteProvider_.get(), &AutoSignQuoteProvider::autoSignStateChanged, this, &AutoSignQuoteWidget::onAutoSignStateChanged);

   connect(autoSignQuoteProvider_.get(), &AutoSignQuoteProvider::aqScriptLoaded, this, &AutoSignQuoteWidget::onAqScriptLoaded);
   connect(autoSignQuoteProvider_.get(), &AutoSignQuoteProvider::aqScriptUnLoaded, this, &AutoSignQuoteWidget::onAqScriptUnloaded);
   connect(autoSignQuoteProvider_.get(), &AutoSignQuoteProvider::aqHistoryChanged, this, &AutoSignQuoteWidget::aqFillHistory);

   ui_->labelAutoSignWalletName->setText(autoSignQuoteProvider_->getAutoSignWalletName());
}

AutoSignQuoteWidget::~AutoSignQuoteWidget() = default;

void AutoSignQuoteWidget::onAutoQuoteToggled()
{
   bool isValidScript = (ui_->comboBoxAQScript->currentIndex() > kSelectAQFileItemIndex);
   if (ui_->checkBoxAQ->isChecked() && !isValidScript) {
      BSMessageBox question(BSMessageBox::question
         , tr("Try to enable Auto Quoting")
         , tr("Auto Quoting Script is not specified. Do you want to select a script from file?"));
      const bool answerYes = (question.exec() == QDialog::Accepted);
      if (answerYes) {
         const auto scriptFileName = askForAQScript();
         if (scriptFileName.isEmpty()) {
            ui_->checkBoxAQ->setChecked(false);
         } else {
            autoSignQuoteProvider_->initAQ(scriptFileName);
         }
      } else {
         ui_->checkBoxAQ->setChecked(false);
      }
   }

   if (autoSignQuoteProvider_->aqLoaded()) {
      autoSignQuoteProvider_->setAqLoaded(false);
   } else {
      autoSignQuoteProvider_->initAQ(ui_->comboBoxAQScript->currentData().toString());
   }

   validateGUI();
}

void AutoSignQuoteWidget::onAutoSignStateChanged()
{
   ui_->checkBoxAutoSign->setChecked(autoSignQuoteProvider_->autoSignState() == bs::error::ErrorCode::NoError);
   if (autoSignQuoteProvider_->autoSignState() != bs::error::ErrorCode::NoError
       && autoSignQuoteProvider_->autoSignState() != bs::error::ErrorCode::AutoSignDisabled) {
      BSMessageBox(BSMessageBox::warning, tr("Auto Signing")
         , tr("Failed to enable Auto Signing")
         , bs::error::ErrorCodeToString(autoSignQuoteProvider_->autoSignState())).exec();
   }

   ui_->labelAutoSignWalletName->setText(autoSignQuoteProvider_->getAutoSignWalletName());
}

void AutoSignQuoteWidget::onAutoSignQuoteAvailChanged()
{
   ui_->groupBoxAutoSign->setEnabled(autoSignQuoteProvider_->autoSignQuoteAvailable());

   ui_->checkBoxAutoSign->setChecked(false);
   ui_->checkBoxAQ->setChecked(false);

   ui_->labelAutoSignWalletName->setText(autoSignQuoteProvider_->getAutoSignWalletName());
}

void AutoSignQuoteWidget::onAqScriptLoaded()
{
   ui_->checkBoxAQ->setChecked(true);
}

void AutoSignQuoteWidget::onAqScriptUnloaded()
{
   ui_->checkBoxAQ->setChecked(false);
}

void AutoSignQuoteWidget::aqFillHistory()
{
   ui_->comboBoxAQScript->clear();
   int curIndex = 0;
   ui_->comboBoxAQScript->addItem(tr("Select script..."));
   ui_->comboBoxAQScript->addItem(tr("Load new AQ script"));
   const auto scripts = autoSignQuoteProvider_->getAQScripts();
   if (!scripts.isEmpty()) {
      const auto lastScript = autoSignQuoteProvider_->getAQLastScript();
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

void AutoSignQuoteWidget::aqScriptChanged(int curIndex)
{
   if (curIndex < kSelectAQFileItemIndex) {
      return;
   }

   if (curIndex == kSelectAQFileItemIndex) {
      const auto scriptFileName = askForAQScript();

      if (scriptFileName.isEmpty()) {
         aqFillHistory();
         return;
      }

      autoSignQuoteProvider_->initAQ(scriptFileName);
   } else {
      if (autoSignQuoteProvider_->aqLoaded()) {
         autoSignQuoteProvider_->deinitAQ();
      }
   }
}

void AutoSignQuoteWidget::onAutoSignToggled()
{
   if (ui_->checkBoxAutoSign->isChecked()) {
      autoSignQuoteProvider_->tryEnableAutoSign();
   } else {
      autoSignQuoteProvider_->disableAutoSign();
   }
   ui_->checkBoxAutoSign->setChecked(autoSignQuoteProvider_->autoSignState() == bs::error::ErrorCode::NoError);
}

void AutoSignQuoteWidget::validateGUI()
{
   ui_->checkBoxAQ->setChecked(autoSignQuoteProvider_->aqLoaded());
}

QString AutoSignQuoteWidget::askForAQScript()
{
   auto lastDir = autoSignQuoteProvider_->getAQLastDir();
   if (lastDir.isEmpty()) {
      lastDir = autoSignQuoteProvider_->getDefaultScriptsDir();
   }

   auto path = QFileDialog::getOpenFileName(this, tr("Open Auto-quoting script file")
      , lastDir, tr("QML files (*.qml)"));

   if (!path.isEmpty()) {
      autoSignQuoteProvider_->setAQLastDir(path);
   }

   return path;
}
