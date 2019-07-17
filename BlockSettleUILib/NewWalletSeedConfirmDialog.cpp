#include "NewWalletSeedConfirmDialog.h"

#include <QValidator>
#include <QEvent>

#include "BSMessageBox.h"
#include "CoreWallet.h"
#include "PaperBackupWriter.h"
#include "UiUtils.h"
#include "ui_NewWalletSeedConfirmDialog.h"
#include "make_unique.h"
#include "EasyEncValidator.h"


NewWalletSeedConfirmDialog::NewWalletSeedConfirmDialog(const std::string &walletId
   , NetworkType netType
   , const QString &keyLine1, const QString &keyLine2, QWidget *parent) :
   QDialog(parent)
   , ui_(new Ui::NewWalletSeedConfirmDialog)
   , walletId_(walletId)
   , netType_(netType)
   , keyLine1_(keyLine1)
   , keyLine2_(keyLine2)
   , easyCodec_(std::make_shared<EasyCoDec>())
{
   ui_->setupUi(this);
   
   ui_->btnContinue->setEnabled(false);
   connect(ui_->btnContinue, &QPushButton::clicked, this, &NewWalletSeedConfirmDialog::onContinueClicked);
   //connect(ui_->pushButtonBack, &QPushButton::clicked, this, &NewWalletSeedDialog::onBackClicked);
   connect(ui_->lineEditLine1, &QLineEdit::textChanged, this, &NewWalletSeedConfirmDialog::onKeyChanged);
   connect(ui_->lineEditLine2, &QLineEdit::textChanged, this, &NewWalletSeedConfirmDialog::onKeyChanged);
   connect(ui_->btnCancel, &QPushButton::clicked, this, &NewWalletSeedConfirmDialog::reject);

   validator_ = make_unique<EasyEncValidator>(easyCodec_, nullptr, 9, true);
   ui_->lineEditLine1->setValidator(validator_.get());
   ui_->lineEditLine2->setValidator(validator_.get());

}

NewWalletSeedConfirmDialog::~NewWalletSeedConfirmDialog() = default;

void NewWalletSeedConfirmDialog::reject() {
   bool result = MessageBoxWalletCreateAbort(this).exec();
   if (result) {
      QDialog::reject();
   }
}

void NewWalletSeedConfirmDialog::onBackClicked()
{
   //setCurrentPage(Pages::PrintPreview);
}

void NewWalletSeedConfirmDialog::onContinueClicked()
{
   accept();
}

void NewWalletSeedConfirmDialog::validateKeys()
{
   if (!keysAreCorrect_) {
      BSMessageBox messageBox(BSMessageBox::critical, tr("Check failed!")
         , tr("Input values do not match with the original keys. Please make sure the input lines are correct."));
      messageBox.exec();
      return;
   }
   
   accept();
}

void NewWalletSeedConfirmDialog::onKeyChanged(const QString &)
{
   QString inputLine1 = ui_->lineEditLine1->text().trimmed();
   QString inputLine2 = ui_->lineEditLine2->text().trimmed();
   QString keyLine1 = keyLine1_;
   QString keyLine2 = keyLine2_;

   // Remove all spaces just in case.
   inputLine1.remove(QChar::Space);
   inputLine2.remove(QChar::Space);
   keyLine1.remove(QChar::Space);
   keyLine2.remove(QChar::Space);
   
   if (inputLine1 != keyLine1 || inputLine2 != keyLine2) {
      keysAreCorrect_ = false;
   } else {
      EasyCoDec::Data ecData;
      ecData.part1 = inputLine1.toStdString();
      ecData.part2 = inputLine2.toStdString();
      const auto &seed = bs::core::wallet::Seed::fromEasyCodeChecksum(ecData, netType_);
      keysAreCorrect_ = (seed.getWalletId() == walletId_);
   }

   if (inputLine1.size() == validator_->getNumWords() * validator_->getWordSize()) {
      if (inputLine1 != keyLine1) {
         UiUtils::setWrongState(ui_->lineEditLine1, true);
      } else {
         UiUtils::setWrongState(ui_->lineEditLine1, false);
      }
   }
   else {
      UiUtils::setWrongState(ui_->lineEditLine1, false);
   }

   if (inputLine2.size() == validator_->getNumWords() * validator_->getWordSize()) {
      if (inputLine2 != keyLine2) {
         UiUtils::setWrongState(ui_->lineEditLine2, true);
      } else {
         UiUtils::setWrongState(ui_->lineEditLine2, false);
      }
   }
   else {
      UiUtils::setWrongState(ui_->lineEditLine2, false);
   }

   updateState();
}

void NewWalletSeedConfirmDialog::updateState()
{
   ui_->btnContinue->setEnabled(keysAreCorrect_);

   // use this string for testing purposes to skip seed check
   // ui_->btnContinue->setEnabled(true);
}
