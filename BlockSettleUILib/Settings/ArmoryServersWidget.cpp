#include "ArmoryServersWidget.h"
#include "ui_ArmoryServersWidget.h"

const int kArmoryDefaultMainNetPort = 80;

ArmoryServersWidget::ArmoryServersWidget(const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
   , const std::shared_ptr<ApplicationSettings> &appSettings, QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ArmoryServersWidget)
   , armoryServersProvider_(armoryServersProvider)
   , appSettings_(appSettings)
   , armoryServersModel_(new ArmoryServersViewModel(armoryServersProvider))
{
   ui_->setupUi(this);

   ui_->pushButtonConnect->setVisible(false);
   ui_->spinBoxPort->setValue(kArmoryDefaultMainNetPort);

   ui_->tableViewArmory->setModel(armoryServersModel_);
   ui_->tableViewArmory->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
//   int defaultSectionSize = ui_->tableViewArmory->horizontalHeader()->defaultSectionSize();
//   ui_->tableViewArmory->horizontalHeader()->resizeSection(0, defaultSectionSize * 2);
//   ui_->tableViewArmory->horizontalHeader()->resizeSection(1, defaultSectionSize);
//   ui_->tableViewArmory->horizontalHeader()->resizeSection(2, defaultSectionSize);
//   ui_->tableViewArmory->horizontalHeader()->resizeSection(3, defaultSectionSize);
   ui_->tableViewArmory->horizontalHeader()->setStretchLastSection(true);

   isStartupDialog_ = false;

   connect(ui_->pushButtonAddServer, &QPushButton::clicked, this, &ArmoryServersWidget::onAddServer);
   connect(ui_->pushButtonDeleteServer, &QPushButton::clicked, this, &ArmoryServersWidget::onDeleteServer);
   connect(ui_->pushButtonEditServer, &QPushButton::clicked, this, &ArmoryServersWidget::onEdit);
   connect(ui_->pushButtonSelectServer, &QPushButton::clicked, this, &ArmoryServersWidget::onSelect);
   connect(ui_->pushButtonConnect, &QPushButton::clicked, this, &ArmoryServersWidget::onConnect);
   connect(ui_->pushButtonCancelSaveServer, &QPushButton::clicked, this, &ArmoryServersWidget::resetForm);
   connect(ui_->pushButtonSaveServer, &QPushButton::clicked, this, &ArmoryServersWidget::onSave);


   connect(ui_->pushButtonClose, &QPushButton::clicked, this, [this](){
      emit needClose();
   });

   connect(ui_->tableViewArmory->selectionModel(), &QItemSelectionModel::selectionChanged, this,
           [this](const QItemSelection &selected, const QItemSelection &deselected){
      // this check will prevent loop selectionChanged -> setupServerFromSelected -> select -> selectionChanged
      if (deselected.isEmpty()) {
         return;
      }

      bool isEmpty = ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty();
      ui_->pushButtonDeleteServer->setDisabled(isEmpty);
      ui_->pushButtonEditServer->setDisabled(isEmpty);
      ui_->pushButtonConnect->setDisabled(isEmpty);
      ui_->pushButtonSelectServer->setDisabled(isEmpty);

      if (!isEmpty && selected.indexes().first().row() < ArmoryServersProvider::kDefaultServersCount) {
         ui_->pushButtonDeleteServer->setDisabled(true);
         ui_->pushButtonEditServer->setDisabled(true);
      }

      resetForm();

      // save to settings right after row highlight
      setupServerFromSelected(true);
   });

   connect(ui_->comboBoxNetworkType, QOverload<int>::of(&QComboBox::currentIndexChanged),
           [this](int index){
      if (index == 1) {
         ui_->spinBoxPort->setValue(appSettings_->GetDefaultArmoryRemotePort(NetworkType::MainNet));
      }
      else if (index == 2){
         ui_->spinBoxPort->setValue(appSettings_->GetDefaultArmoryRemotePort(NetworkType::TestNet));
      }
   });

   resetForm();

   setRowSelected(armoryServersProvider_->indexOfCurrent());

   ui_->pushButtonDeleteServer->setDisabled(ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty());
   ui_->pushButtonEditServer->setDisabled(ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty());
   if (armoryServersProvider_->indexOfCurrent() < ArmoryServersProvider::kDefaultServersCount) {
      ui_->pushButtonDeleteServer->setDisabled(true);
      ui_->pushButtonEditServer->setDisabled(true);
   }
   // TODO: remove select server button if it's not required anymore
   ui_->pushButtonSelectServer->hide();
}

void ArmoryServersWidget::adaptForStartupDialog()
{
   ui_->widgetControlButtons->hide();
   ui_->tableViewArmory->hideColumn(4);
   ui_->widget->layout()->setContentsMargins(0,0,0,0);
   onExpandToggled();
   isStartupDialog_ = true;
}

void ArmoryServersWidget::setRowSelected(int row)
{
   QModelIndex currentIndex;
   if (armoryServersProvider_->servers().size() >= 0) {
      int indexOfCurrent = row;
      if (indexOfCurrent < 0 || indexOfCurrent >= armoryServersProvider_->servers().size()) {
         indexOfCurrent = 0;
      }
      currentIndex = armoryServersModel_->index(indexOfCurrent, 0);
   }
   ui_->tableViewArmory->selectionModel()->select(currentIndex
                                                  , QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

ArmoryServersWidget::~ArmoryServersWidget() = default;

void ArmoryServersWidget::onAddServer()
{
   if (ui_->lineEditName->text().isEmpty() || ui_->lineEditAddress->text().isEmpty())
      return;
   if (ui_->comboBoxNetworkType->currentIndex() == 0)
      return;
   if (ui_->spinBoxPort->value() == 0)
      return;

   ArmoryServer server;
   server.name = ui_->lineEditName->text();
   server.netType = static_cast<NetworkType>(ui_->comboBoxNetworkType->currentIndex() - 1);
   server.armoryDBIp = ui_->lineEditAddress->text();
   server.armoryDBPort = ui_->spinBoxPort->value();
   server.armoryDBKey = ui_->lineEditKey->text();

   bool ok = armoryServersProvider_->add(server);
   if (ok) {
      resetForm();
      setRowSelected(armoryServersProvider_->servers().size() - 1);
      setupServerFromSelected(true);
   }
}

void ArmoryServersWidget::onDeleteServer()
{
   if (ui_->tableViewArmory->selectionModel()->selectedRows(0).isEmpty()) {
      return;
   }

   int selectedRow = ui_->tableViewArmory->selectionModel()->selectedRows(0).first().row();
   // dont delete default servers
   if (selectedRow < ArmoryServersProvider::kDefaultServersCount) {
      return;
   }
   armoryServersProvider_->remove(selectedRow);
   setRowSelected(0);
   setupServerFromSelected(true);
}

void ArmoryServersWidget::onEdit()
{
   if (ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty()) {
      return;
   }

   int index = ui_->tableViewArmory->selectionModel()->selectedIndexes().first().row();
   if (index >= armoryServersProvider_->servers().size()) {
      return;
   }

   ArmoryServer server = armoryServersProvider_->servers().at(index);
   ui_->stackedWidgetAddSave->setCurrentWidget(ui_->pageSaveServerButton);

   ui_->lineEditName->setText(server.name);
   ui_->comboBoxNetworkType->setCurrentIndex(static_cast<int>(server.netType) + 1);
   ui_->lineEditAddress->setText(server.armoryDBIp);
   ui_->spinBoxPort->setValue(server.armoryDBPort);
   ui_->lineEditKey->setText(server.armoryDBKey);
}

void ArmoryServersWidget::onSelect()
{
   setupServerFromSelected(true);
}

void ArmoryServersWidget::onSave()
{
   if (ui_->comboBoxNetworkType->currentIndex() == 0)
      return;
   if (ui_->spinBoxPort->value() == 0)
      return;

   if (ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty()) {
      return;
   }

   int index = ui_->tableViewArmory->selectionModel()->selectedIndexes().first().row();
   if (index >= armoryServersProvider_->servers().size()) {
      return;
   }

   ArmoryServer server;
   server.name = ui_->lineEditName->text();
   server.netType = static_cast<NetworkType>(ui_->comboBoxNetworkType->currentIndex() - 1);
   server.armoryDBIp = ui_->lineEditAddress->text();
   server.armoryDBPort = ui_->spinBoxPort->value();
   server.armoryDBKey = ui_->lineEditKey->text();

   bool ok = armoryServersProvider_->replace(index, server);
   if (ok) {
      resetForm();
      setRowSelected(armoryServersProvider_->indexOfCurrent());
   }
}

void ArmoryServersWidget::onConnect()
{
   //   emit reconnectArmory();
}

void ArmoryServersWidget::onExpandToggled()
{
   isExpanded_ = !isExpanded_;
   ui_->widgetConfigure->setVisible(isExpanded_);

   ui_->tableViewArmory->setColumnHidden(0, !isExpanded_);
   ui_->tableViewArmory->setColumnHidden(2, !isExpanded_);
   ui_->tableViewArmory->setColumnHidden(3, !isExpanded_);
   ui_->tableViewArmory->setColumnHidden(4, !isExpanded_);
   ui_->tableViewArmory->setColumnHidden(5, !isExpanded_);

   ui_->tableViewArmory->setRowHidden(2, !isExpanded_);
   ui_->tableViewArmory->setRowHidden(3, !isExpanded_);
}

void ArmoryServersWidget::setupServerFromSelected(bool needUpdate)
{
   if (ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty()) {
      return;
   }

   int index = ui_->tableViewArmory->selectionModel()->selectedIndexes().first().row();
   if (index >= armoryServersProvider_->servers().size()) {
      return;
   }

   armoryServersProvider_->setupServer(index, needUpdate);
   setRowSelected(armoryServersProvider_->indexOfCurrent());
}

void ArmoryServersWidget::resetForm()
{
   ui_->stackedWidgetAddSave->setCurrentWidget(ui_->pageAddServerButton);

   ui_->lineEditName->clear();
   ui_->comboBoxNetworkType->setCurrentIndex(0);
   ui_->lineEditAddress->clear();
   ui_->spinBoxPort->setValue(0);
   ui_->spinBoxPort->setSpecialValueText(tr(" "));
   ui_->lineEditKey->clear();
}

bool ArmoryServersWidget::isExpanded() const
{
   return isExpanded_;
}
