#ifndef __CHAT_MESSAGES_VIEW_MODEL_H__
#define __CHAT_MESSAGES_VIEW_MODEL_H__


#include <QAbstractTableModel>
#include <QMap>
#include <QVector>
#include <QDateTime>

#include <memory>


class ChatMessagesViewModel : public QAbstractTableModel
{
   Q_OBJECT

public:

   ChatMessagesViewModel(QObject* parent = nullptr);
   ~ChatMessagesViewModel() noexcept override = default;

   ChatMessagesViewModel(const ChatMessagesViewModel&) = delete;
   ChatMessagesViewModel& operator = (const ChatMessagesViewModel&) = delete;

   ChatMessagesViewModel(ChatMessagesViewModel&&) = delete;
   ChatMessagesViewModel& operator = (ChatMessagesViewModel&&) = delete;

public:
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
   void onMessagesBeginUpdate(int count);
   void onSequentialMessageUpdate(const QDateTime&, const QString& messageText);
   void onMessagesEndUpdate();

   void onSingleMessageUpdate(const QDateTime&, const QString& messageText);

private:
   std::vector<std::pair<QDateTime, QString>> messages_;

};

#endif
