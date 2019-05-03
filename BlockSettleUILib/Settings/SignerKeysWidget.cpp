#include "SignerKeysWidget.h"
#include "ui_SignerKeysWidget.h"
#include <QDebug>

SignerKeysWidget::SignerKeysWidget(std::shared_ptr<ApplicationSettings> appSettings, QWidget *parent) :
   QWidget(parent)
   , appSettings_(appSettings)
   , ui_(new Ui::SignerKeysWidget)
   , signerKeysModel_(new SignerKeysModel(appSettings))
{
   ui_->setupUi(this);

   ui_->tableViewSignerKeys->setModel(signerKeysModel_);
   ui_->tableViewSignerKeys->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->tableViewSignerKeys->horizontalHeader()->setStretchLastSection(true);

   connect(ui_->pushButtonAddSignerKey, &QPushButton::clicked, this, &SignerKeysWidget::onAddSignerKey);
   connect(ui_->pushButtonDeleteSignerKey, &QPushButton::clicked, this, &SignerKeysWidget::onDeleteSignerKey);
   connect(ui_->pushButtonEditSignerKey, &QPushButton::clicked, this, &SignerKeysWidget::onEdit);
   connect(ui_->pushButtonCancelSaveSignerKey, &QPushButton::clicked, this, &SignerKeysWidget::resetForm);
   connect(ui_->pushButtonSaveSignerKey, &QPushButton::clicked, this, &SignerKeysWidget::onSave);


   connect(ui_->pushButtonClose, &QPushButton::clicked, this, [this](){
      emit needClose();
   });

   connect(ui_->tableViewSignerKeys->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      [this](const QItemSelection &selected, const QItemSelection &deselected){
      ui_->pushButtonDeleteSignerKey->setDisabled(ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty());
      ui_->pushButtonEditSignerKey->setDisabled(ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty());

      resetForm();
   });

   resetForm();
}

SignerKeysWidget::~SignerKeysWidget() = default;

void SignerKeysWidget::onAddSignerKey()
{
   if (ui_->lineEditName->text().isEmpty() || ui_->lineEditAddress->text().isEmpty())
      return;


   SignerKey signerKey;
   signerKey.name = ui_->lineEditName->text();
   signerKey.address = ui_->lineEditAddress->text();
   signerKey.key = ui_->lineEditKey->text();

   signerKeysModel_->addSignerPubKey(signerKey);
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

   signerKeysModel_->deleteSignerPubKey(selectedRow);
}

void SignerKeysWidget::onEdit()
{
   if (ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty()) {
      return;
   }

   int index = ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().first().row();
   if (index >= signerKeysModel_->signerPubKeys().size()) {
      return;
   }

   SignerKey signerKey = signerKeysModel_->signerPubKeys().at(index);
   ui_->stackedWidgetAddSave->setCurrentWidget(ui_->pageSaveSignerKeyButton);

   ui_->lineEditName->setText(signerKey.name);
   ui_->lineEditAddress->setText(signerKey.address);
   ui_->lineEditKey->setText(signerKey.key);
}

void SignerKeysWidget::onSave()
{
   if (ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().isEmpty()) {
      return;
   }

   int index = ui_->tableViewSignerKeys->selectionModel()->selectedIndexes().first().row();
   SignerKey signerKey;
   signerKey.name = ui_->lineEditName->text();
   signerKey.address = ui_->lineEditAddress->text();
   signerKey.key = ui_->lineEditKey->text();

   signerKeysModel_->editSignerPubKey(index, signerKey);
}

void SignerKeysWidget::resetForm()
{
   ui_->stackedWidgetAddSave->setCurrentWidget(ui_->pageAddSignerKeyButton);

   ui_->lineEditName->clear();
   ui_->lineEditAddress->clear();
   ui_->lineEditKey->clear();
}
