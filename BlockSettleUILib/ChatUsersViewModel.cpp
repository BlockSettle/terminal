#include <QColor>

#include "ChatUsersViewModel.h"
#include "ChatClient.h"

ChatUsersViewModel::ChatUsersViewModel(QObject* parent)
   : QAbstractTableModel(parent)
{
}

QString ChatUsersViewModel::resolveUser(const QModelIndex &index) const
{
   if ((index.row() < 0) || (index.row() >= users_.size())) {
      return {};
   }
   return users_[index.row()]->userId();
}

int ChatUsersViewModel::columnCount(const QModelIndex &/*parent*/) const
{
   return 1;
}

int ChatUsersViewModel::rowCount(const QModelIndex &/*parent*/) const
{
   return users_.size();
}

QVariant ChatUsersViewModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
   return QVariant();
}

QVariant ChatUsersViewModel::data(const QModelIndex &index, int role) const
{
   if (index.row() >= users_.size())
      return QVariant();

   switch (role)
   {
      case Qt::DisplayRole:
         return resolveUser(index);

      case UserConnectionStatusRole:
         return QVariant::fromValue(users_[index.row()]->userConnectionStatus());

      case UserStateRole:
         return QVariant::fromValue(users_[index.row()]->userState());

      case UserNameRole:
         return users_[index.row()]->userName();

      case HaveNewMessageRole:
         return users_[index.row()]->haveNewMessage();
   }

   return QVariant();
}

void ChatUsersViewModel::onUserDataListChanged(const ChatUserDataListPtr &chatUserDataListPtr)
{
   beginResetModel();
   users_.clear();
   users_.reserve(chatUserDataListPtr.size());
   for (const auto &userDataPtr : chatUserDataListPtr) {
      users_.push_back(std::move(userDataPtr));
   }
   endResetModel();
}

