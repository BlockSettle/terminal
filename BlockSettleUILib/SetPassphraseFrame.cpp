/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SetPassphraseFrame.h"
#include "ui_SetPassphraseFrame.h"

SetPassphraseFrame::SetPassphraseFrame(QWidget* parent)
   : QWizardPage(parent)
   , ui_(new Ui::SetPassphraseFrame())
{
   ui_->setupUi(this);
   registerField(QLatin1String("passphrase"), ui_->lineEditNewPassphrase);
   connect(ui_->lineEditNewPassphrase, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);
   connect(ui_->lineEditRepeatPassphrase, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);

   connect(this, &QWizardPage::completeChanged, [=]() {
      if (isComplete())
      {
         ui_->labelStatus->setText(tr("Passphrases match!"));
      }
      else
      {
         if (ui_->lineEditNewPassphrase->text().size() < 6)
         {
            ui_->labelStatus->setText(tr("Passphrase is too short."));
         }
         else
         {
            ui_->labelStatus->setText(tr("Passphrases do not match."));
         }
      }
   });
}

SetPassphraseFrame::~SetPassphraseFrame()
{}

bool SetPassphraseFrame::isComplete() const
{
   return ui_->lineEditNewPassphrase->text().size() >= 6 &&
          ui_->lineEditNewPassphrase->text() == ui_->lineEditRepeatPassphrase->text();
}
