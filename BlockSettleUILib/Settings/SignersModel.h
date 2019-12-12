/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SIGNERS_MODEL_H__
#define __SIGNERS_MODEL_H__

#include <QAbstractTableModel>
#include <memory>

#include "BinaryData.h"
#include "ApplicationSettings.h"
#include "SignersProvider.h"

class SignersModel : public QAbstractTableModel
{
public:
   SignersModel(const std::shared_ptr<SignersProvider> &signersProvider
                          , QObject *parent = nullptr);
   ~SignersModel() noexcept = default;

   SignersModel(const SignersModel&) = delete;
   SignersModel& operator = (const SignersModel&) = delete;

   SignersModel(SignersModel&&) = delete;
   SignersModel& operator = (SignersModel&&) = delete;

   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

   void setHighLightSelectedServer(bool highLightSelectedServer);
   void setSingleColumnMode(bool singleColumnMode);

public slots:
   void update();

private:
   std::shared_ptr<SignersProvider> signersProvider_;
   QList<SignerHost> signers_;

   bool highLightSelectedServer_ = true;
   bool singleColumnMode_ = false;

   enum SignersViewColumns : int
   {
      ColumnName,
      ColumnAddress,
      ColumnPort,
      ColumnKey,
      ColumnsCount
   };
};

#endif // __SIGNERS_MODEL_H__
