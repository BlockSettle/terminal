/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#include <memory>

#include <QAbstractListModel>
#include "ArmorySettings.h"

namespace spdlog {
   class logger;
}

class ArmoryServersModel: public QAbstractListModel
{
   Q_OBJECT
   Q_PROPERTY(int current     READ current      WRITE setCurrent     NOTIFY currentChanged)
   Q_PROPERTY(int connected   READ connected    NOTIFY connectedChanged)
   Q_PROPERTY(int rowCount    READ rowCount     NOTIFY rowCountChanged)

public:
   enum TableRoles {
      TableDataRole = Qt::UserRole + 1, NameRole, NetTypeRole, AddressRole,
      PortRole, KeyRole, DefaultServerRole, CurrentServerRole
   };
   Q_ENUM(TableRoles)
   
   ArmoryServersModel(const std::shared_ptr<spdlog::logger>&, QObject* parent = nullptr);

   void setCurrent (int value);
   void setData(int curIdx, int connIdx, const std::vector<ArmoryServer>&);
   void add(const ArmoryServer&);
   // netType==0 => MainNet, netType==1 => TestNet
   Q_INVOKABLE void add(QString name, QString armoryDBIp, int armoryDBPort, int netType, QString armoryDBKey);
   Q_INVOKABLE bool del(int idx);
   auto data() const { return data_; }
   auto data(int idx) const { return data_.at(idx); }

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
   QHash<int, QByteArray> roleNames() const override;

   bool isEditable(int row) const;

signals:
   void changed(const QModelIndex&, const QVariant&);
   void currentChanged(int index);
   void connectedChanged();
   void rowCountChanged();

private:
   int current() const { return current_; }
   int connected() const { return connected_; }

private:
   std::shared_ptr<spdlog::logger>  logger_;
   int current_{ -1 };
   int connected_{ -1 };
   std::vector<ArmoryServer> data_;
};
