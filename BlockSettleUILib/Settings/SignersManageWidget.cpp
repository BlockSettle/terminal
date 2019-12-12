/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SignersManageWidget.h"
#include "ui_SignersManageWidget.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <SecureBinaryData.h>

const QRegExp kRxAddress(QStringLiteral(R"(^(((?:(?:25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.){3}(?:25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9]))|(^(([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z]|[A-Za-z][A-Za-z0-9\-]*[A-Za-z0-9])$))$)"));

SignerKeysWidget::SignerKeysWidget(const std::shared_ptr<SignersProvider> &signersProvider
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , QWidget *parent) :
   QWidget(parent)
   , appSettings_(appSettings)
   , signersProvider_(signersProvider)
   , ui_(new Ui::SignerKeysWidget)
   , signersModel_(new SignersModel(signersProvider))
{
   ui_->setupUi(this);

   ui_->spinBoxPort->setMinimum(0);
   ui_->spinBoxPort->setMaximum(USHRT_MAX);

   ui_->tableViewSignerKeys->setModel(signersModel_);

   int defaultSectionSize = ui_->tableViewSignerKeys->horizontalHeader()->defaultSectionSize();
   ui_->tableViewSignerKeys->horizontalHeader()->resizeSection(0, defaultSectionSize * 2);
   ui_->tableViewSignerKeys->horizontalHeader()->resizeSection(1, defaultSectionSize);
   ui_->tableViewSignerKeys->horizontalHeader()->resizeSection(2, defaultSectionSize);
   ui_->tableViewSignerKeys->horizontalHeader()->setStretchLastSection(true);

   connect(ui_->pushButtonAddSignerKey, &QPushButton::clicked, this, &SignerKeysWidget::onAddSignerKey);
   connect(ui_->pushButtonDeleteSignerKey, &QPushButton::clicked, this, &SignerKeysWidget::onDeleteSignerKey);
   connect(ui_->pushButtonEditSignerKey, &QPushButton::clicked, this, &SignerKeysWidget::onEdit);
   connect(ui_->pushButtonCancelSaveSignerKey, &QPushButton::clicked, this, &SignerKeysWidget::resetForm);
   connect(ui_->pushButtonSaveSignerKey, &QPushButton::clicked, this, &SignerKeysWidget::onSave);
   connect(ui_->pushButtonSelect, &QPushButton::clicked, this, &SignerKeysWidget::onSelect);
   connect(ui_->pushButtonKeyImport, &QPushButton::clicked, this, &SignerKeysWidget::onKeyImport);

   connect(ui_->lineEditName, &QLineEdit::textChanged, this, &SignerKeysWidget::onFormChanged);
   connect(ui_->lineEditAddress, &QLineEdit::textChanged, this, &SignerKeysWidget::onFormChanged);
   connect(ui_->spinBoxPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &SignerKeysWidget::onFormChanged);

   connect(ui_->pushButtonClose, &QPushButton::clicked, this, [this](){
      emit needClose();
   });

   connect(ui_->tableViewSignerKeys->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      [this](const QItemSelection &selected, const QItemSelection &deselected){
      // this check will prevent loop selectionChanged -> setupSignerFromSelected -> select -> selectionChanged
      if (deselected.isEmpty()) {
         return;
      }

      ui_->pushButtonSelect->setDisabled(ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty());
      ui_->pushButtonDeleteSignerKey->setDisabled(ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty());
      ui_->pushButtonEditSignerKey->setDisabled(ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty());

      if (selected.indexes().first().row() == 0) {
         ui_->pushButtonDeleteSignerKey->setDisabled(true);
         ui_->pushButtonEditSignerKey->setDisabled(true);
      }

      resetForm();

      // save to settings right after row highlight
      setupSignerFromSelected(true);
   });

   resetForm();

   setRowSelected(signersProvider->indexOfCurrent());
   ui_->pushButtonDeleteSignerKey->setDisabled(ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty());
   ui_->pushButtonEditSignerKey->setDisabled(ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty());

   if (signersProvider->indexOfCurrent() == 0) {
      ui_->pushButtonDeleteSignerKey->setDisabled(true);
      ui_->pushButtonEditSignerKey->setDisabled(true);
   }

   auto validator = new QRegExpValidator(this);
   validator->setRegExp(kRxAddress);
   ui_->lineEditAddress->setValidator(validator);
   onFormChanged();

   // TODO: remove select signer button if it's not required anymore
   ui_->pushButtonSelect->hide();
}

void SignerKeysWidget::setRowSelected(int row)
{
   QModelIndex currentIndex;
   if (signersProvider_->signers().size() >= 0) {
      int indexOfCurrent = row;
      if (indexOfCurrent < 0 || indexOfCurrent >= signersProvider_->signers().size()) {
         indexOfCurrent = 0;
      }
      currentIndex = signersModel_->index(indexOfCurrent, 0);
   }
   ui_->tableViewSignerKeys->selectionModel()->select(currentIndex
                                                      , QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

SignerKeysWidget::~SignerKeysWidget() = default;

void SignerKeysWidget::onAddSignerKey()
{
   if (ui_->lineEditName->text().isEmpty() || ui_->lineEditAddress->text().isEmpty())
      return;


   SignerHost signerHost;
   signerHost.name = ui_->lineEditName->text();
   signerHost.address = ui_->lineEditAddress->text();
   signerHost.port = ui_->spinBoxPort->value();
   signerHost.key = ui_->lineEditKey->text();

   signersProvider_->add(signerHost);
   resetForm();

   setRowSelected(signersProvider_->signers().size() - 1);
   setupSignerFromSelected(true);
}

void SignerKeysWidget::onDeleteSignerKey()
{
   if (ui_->tableViewSignerKeys->selectionModel()->selectedRows(0).isEmpty()) {
      return;
   }

   int selectedRow = ui_->tableViewSignerKeys->selectionModel()->selectedRows(0).first().row();
   if (selectedRow < 0) {
      return;
   }

   signersProvider_->remove(selectedRow);
   resetForm();

   setRowSelected(0);
   setupSignerFromSelected(true);
}

void SignerKeysWidget::onEdit()
{
   if (ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty()) {
      return;
   }

   int index = ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().first().row();
   if (index >= signersProvider_->signers().size()) {
      return;
   }

   SignerHost signerHost = signersProvider_->signers().at(index);
   ui_->stackedWidgetAddSave->setCurrentWidget(ui_->pageSaveSignerKeyButton);

   ui_->lineEditName->setText(signerHost.name);
   ui_->lineEditAddress->setText(signerHost.address);
   ui_->spinBoxPort->setValue(signerHost.port);
   ui_->lineEditKey->setText(signerHost.key);
}

void SignerKeysWidget::onSave()
{
   if (ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty()) {
      return;
   }

   int index = ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().first().row();
   SignerHost signerHost;
   signerHost.name = ui_->lineEditName->text();
   signerHost.address = ui_->lineEditAddress->text();
   signerHost.port = ui_->spinBoxPort->value();
   signerHost.key = ui_->lineEditKey->text();

   signersProvider_->replace(index, signerHost);
   resetForm();
}

void SignerKeysWidget::resetForm()
{
   ui_->stackedWidgetAddSave->setCurrentWidget(ui_->pageAddSignerKeyButton);

   ui_->lineEditName->clear();
   ui_->lineEditAddress->clear();
   ui_->lineEditKey->clear();
   ui_->spinBoxPort->setValue(23456);
}

void SignerKeysWidget::onFormChanged()
{
   bool acceptable = ui_->lineEditAddress->hasAcceptableInput();
   bool exists = false;
   bool valid = false;
   if (acceptable) {
      SignerHost signerHost;
      signerHost.name = ui_->lineEditName->text();
      signerHost.address = ui_->lineEditAddress->text();
      signerHost.port = ui_->spinBoxPort->value();
      signerHost.key = ui_->lineEditKey->text();
      valid = signerHost.isValid();
      if (valid) {
         exists = signersProvider_->indexOf(signerHost.name) != -1
               || signersProvider_->indexOf(signerHost) != -1;
      }
   }
   ui_->pushButtonAddSignerKey->setEnabled(valid && acceptable && !exists);
}

void SignerKeysWidget::onSelect()
{
   setupSignerFromSelected(true);
}

void SignerKeysWidget::onKeyImport()
{
   QString fileName = QFileDialog::getOpenFileName(this
      , tr("Open Key File"),  QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
      , tr("Key Files (*.pub *.key);;All files (*.*)"));

   QFile file(fileName);
   if (file.open(QIODevice::ReadOnly)) {
      SecureBinaryData key = SecureBinaryData(file.readAll().constData());
      ui_->lineEditKey->setText(QString::fromStdString(key.toHexStr()));
   }
}

void SignerKeysWidget::setupSignerFromSelected(bool needUpdate)
{
   if (ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty()) {
      return;
   }

   int index = ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().first().row();
   if (index >= signersProvider_->signers().size()) {
      return;
   }

   signersProvider_->setupSigner(index, needUpdate);
   setRowSelected(signersProvider_->indexOfCurrent());
}
