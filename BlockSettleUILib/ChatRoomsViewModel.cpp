#include "ChatRoomsViewModel.h"

ChatRoomsViewModel::ChatRoomsViewModel(QObject *parent) : QAbstractTableModel(parent)
{
   
}

QString ChatRoomsViewModel::resolveRoomDisplay(const QModelIndex& index) const
{
   if ((index.row() < 0) || (index.row() >= _rooms.size())) {
      return {};
   }
   const auto room = _rooms[index.row()];
   return room->getTitle().isEmpty()?room->getId():room->getTitle();
}

QString ChatRoomsViewModel::resolveRoom(const QModelIndex& index) const
{
   if ((index.row() < 0) || (index.row() >= _rooms.size())) {
      return {};
   }
   return _rooms[index.row()]->getId();
}

void ChatRoomsViewModel::onRoomsDataListChanged(const QList<std::shared_ptr<Chat::ChatRoomData> >& roomsDataList)
{
   beginResetModel();
   _rooms.clear();
   _rooms.reserve(roomsDataList.size());
   for (const auto &dataPtr : roomsDataList) {
      _rooms.push_back(std::move(dataPtr));
   }
   endResetModel();
}

int ChatRoomsViewModel::rowCount(const QModelIndex& /*parent*/) const
{
   return _rooms.size();
}

int ChatRoomsViewModel::columnCount(const QModelIndex& /*parent*/) const
{
   return 1;
}

QVariant ChatRoomsViewModel::data(const QModelIndex& index, int role) const
{
   if (index.row() >= _rooms.size())
      return QVariant();

   switch (role)
   {
      case Qt::DisplayRole:
         return resolveRoomDisplay(index);
      case IdentifierRole:
         return resolveRoom(index);
   }

   return QVariant();
}

QVariant ChatRoomsViewModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
   return QVariant();
}
