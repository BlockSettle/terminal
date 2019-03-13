#include "ChatRoomsViewModel.h"

ChatRoomsViewModel::ChatRoomsViewModel(QObject *parent) : QAbstractTableModel(parent)
{
   
}

QString ChatRoomsViewModel::resolveRoomDisplay(const QModelIndex& index) const
{
   if ((index.row() < 0) || (index.row() >= rooms_.size())) {
      return {};
   }
   const auto room = rooms_[index.row()];
   return room->getTitle().isEmpty()?room->getId():room->getTitle();
}

QString ChatRoomsViewModel::resolveRoom(const QModelIndex& index) const
{
   if ((index.row() < 0) || (index.row() >= rooms_.size())) {
      return {};
   }
   return rooms_[index.row()]->getId();
}

void ChatRoomsViewModel::onRoomsDataListChanged(const QList<std::shared_ptr<Chat::ChatRoomData> >& roomsDataList)
{
   beginResetModel();
   rooms_.clear();
   rooms_.reserve(roomsDataList.size());
   for (const auto &dataPtr : roomsDataList) {
      rooms_.push_back(std::move(dataPtr));
   }
   endResetModel();
}

int ChatRoomsViewModel::rowCount(const QModelIndex& /*parent*/) const
{
   return rooms_.size();
}

int ChatRoomsViewModel::columnCount(const QModelIndex& /*parent*/) const
{
   return 1;
}

QVariant ChatRoomsViewModel::data(const QModelIndex& index, int role) const
{
   if (index.row() >= rooms_.size())
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
