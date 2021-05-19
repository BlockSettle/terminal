/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SIGNERS_MANAGE_WIDGET_H
#define SIGNERS_MANAGE_WIDGET_H

#include <QItemSelectionModel>
#include <QWidget>
#include <ApplicationSettings.h>

#include "SignersModel.h"

namespace Ui {
class SignerKeysWidget;
}

class SignerKeysWidget : public QWidget
{
   Q_OBJECT

public:
   [[deprecated]] explicit SignerKeysWidget(const std::shared_ptr<SignersProvider> &signersProvider
      , const std::shared_ptr<ApplicationSettings> &appSettings, QWidget *parent = nullptr);
   explicit SignerKeysWidget(QWidget* parent = nullptr);
   ~SignerKeysWidget();

   void setRowSelected(int row);

   void onSignerSettings(const QList<SignerHost>&, int idxCur);

public slots:
   void onAddSignerKey();
   void onDeleteSignerKey();
   void onEdit();
   void onSave();
   void onSelect();
   void onKeyImport();

signals:
   void needClose();
   void addSigner(const SignerHost&);
   void delSigner(int);
   void updSigner(int, const SignerHost&);

private slots:
   void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

private:
   void setupSignerFromSelected(bool needUpdate);

private slots:
   void resetForm();
   void onFormChanged();

private:
   std::unique_ptr<Ui::SignerKeysWidget> ui_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<SignersProvider> signersProvider_;
   QList<SignerHost> signers_;

   SignersModel* signersModel_{ nullptr };
};

#endif // SIGNERS_MANAGE_WIDGET_H
