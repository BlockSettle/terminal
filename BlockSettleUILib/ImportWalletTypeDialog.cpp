#include "ImportWalletTypeDialog.h"
#include "ui_ImportWalletTypeDialog.h"

#include <QFileDialog>
#include <QResizeEvent>
#include <QStandardPaths>

#include "EasyEncValidator.h"
#include "MessageBoxCritical.h"
#include "MetaData.h"


ImportWalletTypeDialog::ImportWalletTypeDialog(QWidget* parent)
  : QDialog(parent)
   , ui_(new Ui::ImportWalletTypeDialog())
   , easyCodec_(std::make_shared<EasyCoDec>())
{
   ui_->setupUi(this);

   QFontMetrics fm(ui_->lineSeed1->font());
   auto seedSample = QString::fromStdString("wwww wwww wwww wwww wwww wwww wwww wwww wwww");
   auto seedWidth = fm.width(seedSample);

   // 20 to compensate padding
   ui_->lineSeed1->setMinimumWidth(seedWidth + 20);

   connect(ui_->tabWidget, &QTabWidget::currentChanged, this, &ImportWalletTypeDialog::OnTabTypeChanged);
   connect(ui_->radioButtonPaper, &QRadioButton::clicked, this, &ImportWalletTypeDialog::OnPaperSelected);
   connect(ui_->radioButtonDigital, &QRadioButton::clicked, this, &ImportWalletTypeDialog::OnDigitalSelected);

   connect(ui_->lineSeed1, &QLineEdit::textChanged, this, &ImportWalletTypeDialog::updateImportButton);
   connect(ui_->lineSeed2, &QLineEdit::textChanged, this, &ImportWalletTypeDialog::updateImportButton);
   validator_ = new EasyEncValidator(easyCodec_, nullptr, 9, true);
   ui_->lineSeed1->setValidator(validator_);
   ui_->lineSeed2->setValidator(validator_);

   connect(ui_->pushButtonBrowseForFile, &QPushButton::clicked, this, &ImportWalletTypeDialog::OnSelectFilePressed);
   connect(ui_->pushButtonSelWoFile, &QPushButton::clicked, this, &ImportWalletTypeDialog::OnSelectWoFilePressed);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ImportWalletTypeDialog::reject);
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, &ImportWalletTypeDialog::accept);

   OnPaperSelected();
   ui_->labelWoFilePath->clear();
}

ImportWalletTypeDialog::~ImportWalletTypeDialog()
{
   delete validator_;
}

EasyCoDec::Data ImportWalletTypeDialog::GetSeedData() const
{
   if (ui_->stackedWidgetInputs->currentIndex() == 0) {
      EasyCoDec::Data easyData;
      easyData.part1 = ui_->lineSeed1->text().toStdString();
      easyData.part2 = ui_->lineSeed2->text().toStdString();

      return easyData;
   } else {
      if (!walletData_.id.empty()) {
         return walletData_.seed;
      } else {
         return EasyCoDec::Data{};
      }
   }
}

EasyCoDec::Data ImportWalletTypeDialog::GetChainCodeData() const
{
   if (ui_->stackedWidgetInputs->currentIndex() == 0) {
      // empty for paper backup
      return EasyCoDec::Data{};
   } else {
      if (!walletData_.id.empty()) {
         return walletData_.chainCode;
      } else {
         return EasyCoDec::Data{};
      }
   }
}

std::string ImportWalletTypeDialog::GetName() const
{
   return walletData_.name;
}

std::string ImportWalletTypeDialog::GetDescription() const
{
   return walletData_.description;
}

void ImportWalletTypeDialog::OnTabTypeChanged(int)
{
   importType_ = (ui_->tabWidget->currentWidget() == ui_->tabWow) ? WatchingOnly : Full;
}

void ImportWalletTypeDialog::OnPaperSelected()
{
   ui_->stackedWidgetInputs->setCurrentIndex(0);
   updateImportButton();
}

void ImportWalletTypeDialog::OnDigitalSelected()
{
   ui_->stackedWidgetInputs->setCurrentIndex(1);
   updateImportButton();
}

void ImportWalletTypeDialog::updateImportButton()
{
   if (ui_->tabWidget->currentWidget() == ui_->tabFull) {
      if (ui_->stackedWidgetInputs->currentIndex() == 0) {
         ui_->pushButtonImport->setEnabled(PaperImportOk());
      }
      else {
         ui_->pushButtonImport->setEnabled(DigitalImportOk());
      }
   }
   else {
      ui_->pushButtonImport->setEnabled(WoImportOk());
   }
}

bool ImportWalletTypeDialog::PaperImportOk()
{
   auto line1 = ui_->lineSeed1->text();
   auto line2 = ui_->lineSeed2->text();

   if (!line1.isEmpty() && !line2.isEmpty()) {
      try {
         EasyCoDec::Data easyData;
         easyData.part1 = line1.toStdString();
         easyData.part2 = line2.toStdString();

         bs::wallet::Seed::fromEasyCodeChecksum(easyData, NetworkType::Invalid);

         return true;
      } catch (...) {
         return false;
      }
   }

   return false;
}

bool ImportWalletTypeDialog::DigitalImportOk()
{
   return !walletData_.id.empty();
}

void ImportWalletTypeDialog::resizeEvent(QResizeEvent *event)
{
   ui_->horizontalWidgetHeader->setMinimumWidth(event->size().width());
   ui_->labelHeader->setMinimumWidth(event->size().width());
}

void ImportWalletTypeDialog::OnSelectFilePressed()
{
   auto fileToOpen = QFileDialog::getOpenFileName(this
      , tr("Open Wallet Digital Backup File")
      , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
      , tr("*.wdb"));

   if (!fileToOpen.isEmpty()) {
      ui_->labelFileName->setText(tr("..."));
      walletData_ = {};

      QFile file(fileToOpen);
      if (file.open(QIODevice::ReadOnly)) {
         QByteArray data = file.readAll();
         walletData_ = WalletBackupFile::Deserialize(std::string(data.data(), data.size()));
         if (walletData_.id.empty()) {
            MessageBoxCritical(tr("Backup file corrupted"), tr("Could not load wallet from file"), this).exec();
         } else {
            ui_->labelFileName->setText(fileToOpen);
         }
      } else {
         MessageBoxCritical(tr("Failed to read backup file"), tr("Could not read %1").arg(fileToOpen), this).exec();
      }

      updateImportButton();
   }
}

void ImportWalletTypeDialog::OnSelectWoFilePressed()
{
   woFileName_ = QFileDialog::getOpenFileName(this
      , tr("Open Watching-Only Wallet File")
      , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
      , tr("*.lmdb"));

   if (!woFileName_.isEmpty()) {
      ui_->labelWoFilePath->setText(woFileName_);
      woFileExists_ = false;

      QFile file(woFileName_);
      if (file.exists()) {
         woFileExists_ = true;
      }
      else {
         MessageBoxCritical(tr("Failed to read backup file"), tr("Could not read %1").arg(woFileName_), this).exec();
      }

      updateImportButton();
   }
}
