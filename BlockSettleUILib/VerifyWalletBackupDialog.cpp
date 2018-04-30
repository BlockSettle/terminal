#include "VerifyWalletBackupDialog.h"
#include "ui_VerifyWalletBackupDialog.h"
#include "EasyCoDec.h"
#include "EasyEncValidator.h"
#include "HDNode.h"
#include "HDWallet.h"
#include "MetaData.h"


VerifyWalletBackupDialog::VerifyWalletBackupDialog(const std::shared_ptr<bs::hd::Wallet> &wallet
   , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::VerifyWalletBackupDialog)
   , wallet_(wallet)
   , easyCodec_(std::make_shared<EasyCoDec>())
   , netType_(wallet->getXBTGroupType() == bs::hd::CoinType::Bitcoin_test ? NetworkType::TestNet : NetworkType::MainNet)
{
   ui_->setupUi(this);

   validator_ = new EasyEncValidator(easyCodec_, nullptr, 9, true);
   ui_->lineEditPrivKey1->setValidator(validator_);
   ui_->lineEditPrivKey2->setValidator(validator_);

   connect(ui_->pushButtonClose, &QPushButton::clicked, this, &QDialog::accept);
   connect(ui_->lineEditPrivKey1, &QLineEdit::textEdited, this, &VerifyWalletBackupDialog::onPrivKeyChanged);
   connect(ui_->lineEditPrivKey1, &QLineEdit::editingFinished, this, &VerifyWalletBackupDialog::onPrivKeyChanged);
   connect(ui_->lineEditPrivKey2, &QLineEdit::textEdited, this, &VerifyWalletBackupDialog::onPrivKeyChanged);
   connect(ui_->lineEditPrivKey2, &QLineEdit::editingFinished, this, &VerifyWalletBackupDialog::onPrivKeyChanged);
}

VerifyWalletBackupDialog::~VerifyWalletBackupDialog()
{
   delete validator_;
}

void VerifyWalletBackupDialog::onPrivKeyChanged()
{
   if (ui_->lineEditPrivKey1->text().isEmpty() || ui_->lineEditPrivKey2->text().isEmpty()) {
      ui_->labelResult->setStyleSheet({});
      ui_->labelResult->setText(tr("Please enter private key"));
      return;
   }
   EasyCoDec::Data easyData;
   easyData.part1 = ui_->lineEditPrivKey1->text().toStdString();
   easyData.part2 = ui_->lineEditPrivKey2->text().toStdString();
   try {
      const auto seed = bs::wallet::Seed::fromEasyCodeChecksum(easyData, netType_);
      bs::hd::Wallet newWallet(wallet_->getName(), wallet_->getDesc(), false, seed);
      if (newWallet.getWalletId() == wallet_->getWalletId()) {
         ui_->labelResult->setStyleSheet(QLatin1String("QLabel {color : green;}"));
         ui_->labelResult->setText(tr("Valid"));
         return;
      }
   }
   catch (const std::exception &) {}
   ui_->labelResult->setStyleSheet(QLatin1String("QLabel {color : red;}"));
   ui_->labelResult->setText(tr("Invalid"));
}
