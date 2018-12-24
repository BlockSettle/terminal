#include "ChatMessagesViewModel.h"

#include "ChatClient.h"

#include <QDebug>



ChatMessagesViewModel::ChatMessagesViewModel(QObject* parent)
   : QAbstractTableModel(parent)
{

}


int ChatMessagesViewModel::columnCount(const QModelIndex &parent) const
{
   return 2;
}


int ChatMessagesViewModel::rowCount(const QModelIndex &parent) const
{
   return messages_.size();
}


QVariant ChatMessagesViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   switch (section)
   {
      case 0:
         return tr("Time");

      case 1:
         return tr("Message");

      default:
         break;
   }

   return QVariant();
}


QVariant ChatMessagesViewModel::data(const QModelIndex &index, int role) const
{
   if (role == Qt::DisplayRole)
   {
      switch (index.column())
      {
         case 0:
         {
            auto dateTime = messages_[index.row()].first;

            if (dateTime.date() == QDate::currentDate())
            {
               return dateTime.time().toString(QString::fromUtf8("hh:mm"));
            }
            else
            {
               return dateTime.toString();
            }
         }

         case 1:
            return messages_[index.row()].second;

         default:
            break;
      }
   }
   return QVariant();
}


void ChatMessagesViewModel::onLeaveRoom()
{
   beginResetModel();
   messages_.clear();
   endResetModel();
}


void ChatMessagesViewModel::onMessage(const QDateTime& date, const QString& messageText)
{
   auto rowIdx = static_cast<int>(messages_.size());
   beginInsertRows(QModelIndex(), rowIdx, rowIdx);
   messages_.push_back(std::make_pair(date, messageText));
   endInsertRows();
}
