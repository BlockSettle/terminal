#include "ArmoryServersWidget.h"
#include "ui_ArmoryServersWidget.h"
#include <QDebug>

ArmoryServersWidget::ArmoryServersWidget(const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider, QWidget *parent) :
   QDialog(parent)
   , armoryServersProvider_(armoryServersProvider)
   , ui_(new Ui::ArmoryServersWidget)
   , armoryServersModel(new ArmoryServersViewModel(armoryServersProvider))
{
   ui_->setupUi(this);
   ui_->pushButtonConnect->setVisible(false);

   //ui_->tableViewArmory->horizontalHeader()->setStretchLastSection(true);
   ui_->tableViewArmory->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->tableViewArmory->setModel(armoryServersModel);

   connect(ui_->pushButtonAddServer, &QPushButton::clicked, this, &ArmoryServersWidget::onAddServer);
   connect(ui_->pushButtonDeleteServer, &QPushButton::clicked, this, &ArmoryServersWidget::onDeleteServer);
   connect(ui_->pushButtonConnect, &QPushButton::clicked, this, &ArmoryServersWidget::onConnect);
   connect(ui_->pushButtonClose, &QPushButton::clicked, this, &ArmoryServersWidget::reject);

   connect(ui_->tableViewArmory->selectionModel(), &QItemSelectionModel::selectionChanged, this,
           [this](const QItemSelection &selected, const QItemSelection &deselected){
      ui_->pushButtonDeleteServer->setDisabled(ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty());
      if (selected.indexes().first().row() < ArmoryServersProvider::defaultServersCount) {
         ui_->pushButtonDeleteServer->setDisabled(true);
      }
      ui_->pushButtonConnect->setDisabled(ui_->tableViewArmory->selectionModel()->selectedIndexes().isEmpty());
   });
}

ArmoryServersWidget::~ArmoryServersWidget() = default;

void ArmoryServersWidget::onAddServer()
{
   if (ui_->lineEditName->text().isEmpty() || ui_->lineEditAddress->text().isEmpty())
      return;

   if (ui_->lineEditKey->text().isEmpty() && ui_->comboBoxNetworkType->currentIndex() == 1)
      return;

   ArmoryServer server;
   server.name = ui_->lineEditName->text();
   server.netType = static_cast<NetworkType>(ui_->comboBoxNetworkType->currentIndex());
   server.armoryDBIp = ui_->lineEditAddress->text();
   server.armoryDBPort = ui_->spinBoxPort->value();
   server.armoryDBKey = ui_->lineEditKey->text();

   armoryServersProvider_->add(server);
}

void ArmoryServersWidget::onDeleteServer()
{
   if (ui_->tableViewArmory->selectionModel()->selectedRows(0).isEmpty()) {
      return;
   }
   // dont delete default servers
   if (ui_->tableViewArmory->selectionModel()->selectedRows(0).first().row() < ArmoryServersProvider::defaultServersCount) {
      return;
   }
   armoryServersProvider_->remove(ui_->tableViewArmory->selectionModel()->selectedRows(0).first().row());
}

void ArmoryServersWidget::onConnect()
{
//   emit reconnectArmory();
}
