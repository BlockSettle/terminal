#include "ChatUsersViewModel.h"

#include "ChatClient.h"


#include <QDebug>


ChatUsersViewModel::ChatUsersViewModel(QObject* parent)
   : QAbstractTableModel(parent)
{
}


QString ChatUsersViewModel::resolveUser(const QModelIndex& index)
{
   return userByIndex_[index.row()];
}


QModelIndex ChatUsersViewModel::resolveUser(const QString& userId)
{
    return index(indexByUser_[userId], 0);
}


int ChatUsersViewModel::columnCount(const QModelIndex &parent) const
{
   return 1;
}


int ChatUsersViewModel::rowCount(const QModelIndex &parent) const
{
   return userByIndex_.count();
}


QVariant ChatUsersViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   return QVariant();
}


QVariant ChatUsersViewModel::data(const QModelIndex &index, int role) const
{
   if (role == Qt::DisplayRole) {
      return userByIndex_[index.row()];
   }
   return QVariant();
}


void ChatUsersViewModel::onClear()
{
   beginResetModel();
   userByIndex_.clear();
   indexByUser_.clear();
   endResetModel();
}


void ChatUsersViewModel::onUsersUpdate(const std::vector<std::string>& users)
{
    onClear();

    beginInsertRows(QModelIndex(), 0, users.size());
    foreach(auto userId, users) {

        auto insertingUser = QString::fromStdString(userId);
        indexByUser_[insertingUser] = userByIndex_.size();
        userByIndex_.append(insertingUser);
    }
    endInsertRows();
}
