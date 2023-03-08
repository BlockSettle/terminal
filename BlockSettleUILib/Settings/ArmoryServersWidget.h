/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef ARMORYSERVERSWIDGET_H
#define ARMORYSERVERSWIDGET_H

#include <QItemSelectionModel>
#include <QWidget>
#include <ApplicationSettings.h>

#include "../Core/ArmoryServersProvider.h"
#include "ArmoryServersViewModel.h"

namespace Ui {
class ArmoryServersWidget;
}

class ArmoryServersWidget : public QWidget
{
   Q_OBJECT

public:
   [[deprecated]] explicit ArmoryServersWidget(const std::shared_ptr<ArmoryServersProvider>& armoryServersProvider
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , QWidget *parent = nullptr);
   explicit ArmoryServersWidget(QWidget* parent = nullptr);
   ~ArmoryServersWidget();

   void setRowSelected(int row);

   bool isExpanded() const;

   void onArmoryServers(const QList<ArmoryServer>&, int idxCur, int idxConn);

public slots:
   void onAddServer();
   void onDeleteServer();
   void onEdit();
   void onSelect();
   void onSave();
   void onConnect();

   void onExpandToggled();
   void onFormChanged();

private slots:
   void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
   void onCurIndexChanged(int index);

signals:
   void reconnectArmory();
   void needClose();
   void setServer(int);
   void addServer(const ArmoryServer&);
   void delServer(int);
   void updServer(int, const ArmoryServer&);

private:
   void setupServerFromSelected(bool needUpdate);

private slots:
   void resetForm();
   void updateSaveButton();

private:
   std::unique_ptr<Ui::ArmoryServersWidget> ui_; // The main widget object.
   std::shared_ptr<ArmoryServersProvider> armoryServersProvider_;
   std::shared_ptr<ApplicationSettings> appSettings_;

   ArmoryServersViewModel* armoryServersModel_{ nullptr };
   QList<ArmoryServer>  servers_;
   bool isStartupDialog_ = false;
   bool isExpanded_ = true;
};

#endif // ARMORYSERVERSWIDGET_H
