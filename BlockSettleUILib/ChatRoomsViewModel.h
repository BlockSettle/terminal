#ifndef CHATROOMSVIEWMODEL_H
#define CHATROOMSVIEWMODEL_H

#include <QAbstractItemModel>
#include <QMap>
#include <QVector>

#include <memory>

#include "ChatProtocol/DataObjects.h"

class ChatRoomsViewModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   enum Role
   {
      IdentifierRole = Qt::UserRole
   };
   explicit ChatRoomsViewModel(QObject* parent = nullptr);
   QString resolveRoomDisplay(const QModelIndex &) const;
   QString resolveRoom(const QModelIndex &) const;
signals:
   
public slots:
   void onRoomsDataListChanged(const QList<std::shared_ptr<Chat::ChatRoomData>>& roomsDataList);
   // QAbstractItemModel interface
public:
   int rowCount(const QModelIndex& parent) const;
   int columnCount(const QModelIndex& parent) const;
   QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const;
   
private:
   QList<std::shared_ptr<Chat::ChatRoomData>> _rooms;
};

#endif // CHATROOMSVIEWMODEL_H
