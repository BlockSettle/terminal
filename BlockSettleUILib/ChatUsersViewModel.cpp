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


void ChatUsersViewModel::onUserUpdate(const QString& userId)
{
    qDebug() << "onUserUpdate " << userId;

    if (indexByUser_.contains(userId))
    {
        return;
    }

    indexByUser_[userId] = userByIndex_.count();
    userByIndex_.append(userId);

    emit beginResetModel();
    emit endResetModel();
}
