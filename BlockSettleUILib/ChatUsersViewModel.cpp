#include <QColor>

#include "ChatUsersViewModel.h"
#include "ChatClient.h"

ChatUsersViewModel::ChatUsersViewModel(QObject* parent)
   : QAbstractTableModel(parent)
{
}

QString ChatUsersViewModel::resolveUser(const QModelIndex &index) const
{
   if ((index.row() < 0) || (index.row() >= _users.size())) {
      return {};
   }
   return _users[index.row()]->userId();
}

int ChatUsersViewModel::columnCount(const QModelIndex &/*parent*/) const
{
   return 1;
}

int ChatUsersViewModel::rowCount(const QModelIndex &/*parent*/) const
{
   return _users.size();
}

QVariant ChatUsersViewModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
   return QVariant();
}

QVariant ChatUsersViewModel::data(const QModelIndex &index, int role) const
{
   if (index.row() >= _users.size())
      return QVariant();

   switch (role)
   {
      case Qt::DisplayRole:
         return resolveUser(index);

      case UserConnectionStatusRole:
         return QVariant::fromValue(_users[index.row()]->userConnectionStatus());

      case UserStateRole:
         return QVariant::fromValue(_users[index.row()]->userState());

      case UserNameRole:
         return _users[index.row()]->userName();

      case HaveNewMessageRole:
         return _users[index.row()]->haveNewMessage();
   }

   return QVariant();
}

void ChatUsersViewModel::onUserDataListChanged(const ChatUserDataListPtr &chatUserDataListPtr)
{
   beginResetModel();
   _users.clear();
   _users.reserve(chatUserDataListPtr.size());
   for (const auto &userDataPtr : chatUserDataListPtr) {
      _users.push_back(std::move(userDataPtr));
   }
   endResetModel();
}

