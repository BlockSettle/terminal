#include "VerifyWalletBackupDialog.h"
#include "ui_VerifyWalletBackupDialog.h"
#include "CoreWallet.h"
#include "EasyCoDec.h"
#include "EasyEncValidator.h"
#include "Wallets/SyncHDWallet.h"


VerifyWalletBackupDialog::VerifyWalletBackupDialog(const std::shared_ptr<bs::sync::hd::Wallet> &wallet
                                 , const std::shared_ptr<spdlog::logger> &logger
                                                   , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::VerifyWalletBackupDialog)
   , wallet_(wallet)
   , logger_(logger)
   , netType_(wallet->getXBTGroupType() == bs::hd::CoinType::Bitcoin_test ? NetworkType::TestNet : NetworkType::MainNet)
   , easyCodec_(std::make_shared<EasyCoDec>())
{
   ui_->setupUi(this);

   validator_.reset(new EasyEncValidator(easyCodec_, nullptr, 9, true));
   ui_->lineEditPrivKey1->setValidator(validator_.get());
   ui_->lineEditPrivKey2->setValidator(validator_.get());

   connect(ui_->pushButtonClose, &QPushButton::clicked, this, &QDialog::accept);
   connect(ui_->lineEditPrivKey1, &QLineEdit::textEdited, this, &VerifyWalletBackupDialog::onPrivKeyChanged);
   connect(ui_->lineEditPrivKey1, &QLineEdit::editingFinished, this, &VerifyWalletBackupDialog::onPrivKeyChanged);
   connect(ui_->lineEditPrivKey2, &QLineEdit::textEdited, this, &VerifyWalletBackupDialog::onPrivKeyChanged);
   connect(ui_->lineEditPrivKey2, &QLineEdit::editingFinished, this, &VerifyWalletBackupDialog::onPrivKeyChanged);
}

VerifyWalletBackupDialog::~VerifyWalletBackupDialog() = default;

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
      const auto seed = bs::core::wallet::Seed::fromEasyCodeChecksum(easyData, netType_);
      //TODO: add verification of wallet seed against wallet_->walletId()
      if (seed.privateKey().getSize() == 32) {
         ui_->labelResult->setStyleSheet(QLatin1String("QLabel {color : green;}"));
         ui_->labelResult->setText(tr("Valid"));
         return;
      }
   }
   catch (const std::exception &) {}
   ui_->labelResult->setStyleSheet(QLatin1String("QLabel {color : red;}"));
   ui_->labelResult->setText(tr("Invalid"));
}
