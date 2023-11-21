/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ArmoryServersWidget.h"
#include "ui_ArmoryServersWidget.h"

const int kArmoryDefaultMainNetPort = 80;
const QRegExp kRxAddress(QStringLiteral(R"(^(((?:(?:25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.){3}(?:25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9]))|(^(([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z]|[A-Za-z][A-Za-z0-9\-]*[A-Za-z0-9])$))$)"));

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

   connect(ui_->lineEditAddress, &QLineEdit::textChanged, this, &ArmoryServersWidget::onFormChanged);

   QRegExp rx(kRxAddress);
   ui_->lineEditAddress->setValidator(new QRegExpValidator(rx, this));
   onFormChanged();

   connect(ui_->pushButtonClose, &QPushButton::clicked, this, [this](){
      emit needClose();
   });

   connect(ui_->tableViewArmory->selectionModel(), &QItemSelectionModel::selectionChanged
      , this, &ArmoryServersWidget::onSelectionChanged);
   connect(ui_->comboBoxNetworkType, QOverload<int>::of(&QComboBox::currentIndexChanged)
      , this, &ArmoryServersWidget::onCurIndexChanged);

   resetForm();

   setRowSelected(armoryServersProvider_->indexOfCurrent());

   ui_->pushButtonDeleteServer->setDisabled(ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty());
   ui_->pushButtonEditServer->setDisabled(ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty());
   if (armoryServersProvider_->indexOfCurrent() < ArmoryServersProvider::kDefaultServersCount) {
      ui_->pushButtonDeleteServer->setDisabled(true);
      ui_->pushButtonEditServer->setDisabled(true);
   }

   connect(ui_->lineEditName, &QLineEdit::textEdited, this, &ArmoryServersWidget::updateSaveButton);
   connect(ui_->lineEditAddress, &QLineEdit::textEdited, this, &ArmoryServersWidget::updateSaveButton);
   connect(ui_->comboBoxNetworkType, &QComboBox::currentTextChanged, this, &ArmoryServersWidget::updateSaveButton);
   connect(ui_->spinBoxPort, QOverload<const QString &>::of(&QSpinBox::valueChanged), this, &ArmoryServersWidget::updateSaveButton);

   updateSaveButton();

   // TODO: remove select server button if it's not required anymore
   ui_->pushButtonSelectServer->hide();

   ui_->pushButtonKeyImport->hide();
}

ArmoryServersWidget::ArmoryServersWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::ArmoryServersWidget)
{
   armoryServersModel_ = new ArmoryServersViewModel(this);
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
   connect(ui_->lineEditAddress, &QLineEdit::textChanged, this, &ArmoryServersWidget::onFormChanged);

   QRegExp rx(kRxAddress);
   ui_->lineEditAddress->setValidator(new QRegExpValidator(rx, this));
   onFormChanged();

   connect(ui_->pushButtonClose, &QPushButton::clicked, this, [this]() {
      emit needClose();
   });

   connect(ui_->tableViewArmory->selectionModel(), &QItemSelectionModel::selectionChanged
      , this, &ArmoryServersWidget::onSelectionChanged);
   connect(ui_->comboBoxNetworkType, QOverload<int>::of(&QComboBox::currentIndexChanged)
      , this, &ArmoryServersWidget::onCurIndexChanged);

   resetForm();

   connect(ui_->lineEditName, &QLineEdit::textEdited, this, &ArmoryServersWidget::updateSaveButton);
   connect(ui_->lineEditAddress, &QLineEdit::textEdited, this, &ArmoryServersWidget::updateSaveButton);
   connect(ui_->comboBoxNetworkType, &QComboBox::currentTextChanged, this, &ArmoryServersWidget::updateSaveButton);
   connect(ui_->spinBoxPort, QOverload<const QString&>::of(&QSpinBox::valueChanged), this, &ArmoryServersWidget::updateSaveButton);

   updateSaveButton();

   // TODO: remove select server button if it's not required anymore
   ui_->pushButtonSelectServer->hide();
   ui_->pushButtonKeyImport->hide();
}

void ArmoryServersWidget::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
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
}

void ArmoryServersWidget::setRowSelected(int row)
{
   QModelIndex currentIndex;
   if (armoryServersProvider_ && (armoryServersProvider_->servers().size() >= 0)) {
      int indexOfCurrent = row;
      if (indexOfCurrent < 0 || indexOfCurrent >= armoryServersProvider_->servers().size()) {
         indexOfCurrent = 0;
      }
      currentIndex = armoryServersModel_->index(indexOfCurrent, 0);
   }
   else {
      if (row >= armoryServersModel_->rowCount()) {
         row = 0;
      }
      currentIndex = armoryServersModel_->index(row, 0);
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
   server.name = ui_->lineEditName->text().toStdString();
   server.netType = static_cast<NetworkType>(ui_->comboBoxNetworkType->currentIndex() - 1);
   server.armoryDBIp = ui_->lineEditAddress->text().toStdString();
   server.armoryDBPort = std::to_string(ui_->spinBoxPort->value());
   server.armoryDBKey = ui_->lineEditKey->text().toStdString();

   if (armoryServersProvider_) {
      bool ok = armoryServersProvider_->add(server);
      if (ok) {
         resetForm();
         setRowSelected(armoryServersProvider_->servers().size() - 1);
         setupServerFromSelected(true);
      }
   }
   else {
      emit addServer(server);
      resetForm();
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
   if (armoryServersProvider_) {
      armoryServersProvider_->remove(selectedRow);
      setupServerFromSelected(true);
   }
   else {
      emit delServer(selectedRow);
   }
}

void ArmoryServersWidget::onEdit()
{
   if (ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty()) {
      return;
   }

   int index = ui_->tableViewArmory->selectionModel()->selectedIndexes().first().row();
   ArmoryServer server;
   if (armoryServersProvider_) {
      if (index >= armoryServersProvider_->servers().size()) {
         return;
      }
      server = armoryServersProvider_->servers().at(index);
   }
   else {
      if (index >= servers_.size()) {
         return;
      }
      server = servers_.at(index);  //FIXME: use model instead to retrieve server data
   }

   ui_->stackedWidgetAddSave->setCurrentWidget(ui_->pageSaveServerButton);

   ui_->lineEditName->setText(QString::fromStdString(server.name));
   ui_->comboBoxNetworkType->setCurrentIndex(static_cast<int>(server.netType) + 1);
   ui_->lineEditAddress->setText(QString::fromStdString(server.armoryDBIp));
   ui_->spinBoxPort->setValue(std::stoi(server.armoryDBPort));
   ui_->lineEditKey->setText(QString::fromStdString(server.armoryDBKey));
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
   if (armoryServersProvider_ && (index >= armoryServersProvider_->servers().size())) {
      return;
   }

   ArmoryServer server;
   server.name = ui_->lineEditName->text().toStdString();
   server.netType = static_cast<NetworkType>(ui_->comboBoxNetworkType->currentIndex() - 1);
   server.armoryDBIp = ui_->lineEditAddress->text().toStdString();
   server.armoryDBPort = std::to_string(ui_->spinBoxPort->value());
   server.armoryDBKey = ui_->lineEditKey->text().toStdString();

   if (armoryServersProvider_) {
      bool ok = armoryServersProvider_->replace(index, server);
      if (ok) {
         resetForm();
         setRowSelected(armoryServersProvider_->indexOfCurrent());
      }
   }
   else {
      emit updServer(index, server);
      resetForm();
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
   if (armoryServersProvider_ && (index >= armoryServersProvider_->servers().size())) {
      return;
   }
   if (armoryServersProvider_) {
      armoryServersProvider_->setupServer(index);
      setRowSelected(armoryServersProvider_->indexOfCurrent());
   }
   else {
      emit setServer(index);
   }
}

void ArmoryServersWidget::resetForm()
{
   ui_->stackedWidgetAddSave->setCurrentWidget(ui_->pageAddServerButton);

   ui_->lineEditName->clear();
   ui_->comboBoxNetworkType->setCurrentIndex(0);
   ui_->lineEditAddress->clear();
   ui_->spinBoxPort->setValue(0);
   ui_->spinBoxPort->setSpecialValueText(QLatin1String(" "));
   ui_->lineEditKey->clear();
}

void ArmoryServersWidget::updateSaveButton()
{
   bool saveAllowed = !ui_->lineEditName->text().isEmpty()
      && !ui_->lineEditAddress->text().isEmpty()
      && ui_->comboBoxNetworkType->currentIndex() > 0
      && ui_->spinBoxPort->value() > 0;

   ui_->pushButtonSaveServer->setEnabled(saveAllowed);
   ui_->pushButtonAddServer->setEnabled(saveAllowed);
}

bool ArmoryServersWidget::isExpanded() const
{
   return isExpanded_;
}

void ArmoryServersWidget::onArmoryServers(const QList<ArmoryServer>& servers
   , int idxCur, int idxConn)
{
   servers_ = servers;
   if (idxCur < ArmoryServersProvider::kDefaultServersCount) {
      ui_->pushButtonDeleteServer->setDisabled(true);
      ui_->pushButtonEditServer->setDisabled(true);
   }

   if (armoryServersModel_) {
      armoryServersModel_->onArmoryServers(servers, idxCur, idxConn);
   }
   setRowSelected(idxCur);
}

void ArmoryServersWidget::onFormChanged()
{
   const bool acceptable = ui_->lineEditAddress->hasAcceptableInput();
   bool exists = false;
   bool valid = false;
   if (acceptable) {
      ArmoryServer armoryHost;
      armoryHost.name = ui_->lineEditName->text().toStdString();
      armoryHost.armoryDBIp = ui_->lineEditAddress->text().toStdString();
      armoryHost.armoryDBPort = std::to_string(ui_->spinBoxPort->value());
      armoryHost.armoryDBKey = ui_->lineEditKey->text().toStdString();
      valid = armoryHost.isValid();
      if (valid) {
         if (armoryServersProvider_) {
            exists = armoryServersProvider_->indexOf(QString::fromStdString(armoryHost.name)) != -1
               || armoryServersProvider_->indexOf(armoryHost) != -1;
         }
         else {
            for (int i = 0; i < servers_.size(); ++i) {
               if ((armoryHost.name == servers_.at(i).name)
                  || ((armoryHost.armoryDBIp == servers_.at(i).armoryDBIp) &&
                        (armoryHost.armoryDBPort == servers_.at(i).armoryDBPort))) {
                  exists = true;
                  break;
               }
            }
         }
      }
   }
   ui_->pushButtonAddServer->setEnabled(valid && acceptable && !exists);
   ui_->pushButtonSaveServer->setEnabled(valid && acceptable);
}

void ArmoryServersWidget::onCurIndexChanged(int index)
{
   if (appSettings_) {
      if (index == 1) {
         ui_->spinBoxPort->setValue(appSettings_->GetDefaultArmoryRemotePort(NetworkType::MainNet));
      } else if (index == 2) {
         ui_->spinBoxPort->setValue(appSettings_->GetDefaultArmoryRemotePort(NetworkType::TestNet));
      }
   }
}
