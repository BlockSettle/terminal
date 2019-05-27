#include "SignersManageWidget.h"
#include "ui_SignersManageWidget.h"
#include <QDebug>
#include <QFileDialog>
#include <QStandardPaths>
#include <SecureBinaryData.h>

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
   ui_->tableViewSignerKeys->selectionModel()->select(signersModel_->index(signersProvider->indexOfCurrent(), 0)
      , QItemSelectionModel::Select | QItemSelectionModel::Rows);

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


   connect(ui_->pushButtonClose, &QPushButton::clicked, this, [this](){
      emit needClose();
   });

   connect(ui_->tableViewSignerKeys->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      [this](const QItemSelection &selected, const QItemSelection &deselected){

      ui_->pushButtonSelect->setDisabled(ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty());
      ui_->pushButtonDeleteSignerKey->setDisabled(ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty());
      ui_->pushButtonEditSignerKey->setDisabled(ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty());

      resetForm();

      // save to settings right after row highlight
      if (!ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty()) {
         int index = ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().first().row();

         if (index < signersProvider_->signers().size()) {
            signersProvider_->setupSigner(index, false);
         }
      }
   });

   resetForm();
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

void SignerKeysWidget::onSelect()
{
   if (ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty()) {
      return;
   }

   int index = ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().first().row();
   if (index >= signersProvider_->signers().size()) {
      return;
   }

   signersProvider_->setupSigner(index);
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


