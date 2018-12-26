#ifndef __CHAT_USERS_VIEW_MODEL__
#define __CHAT_USERS_VIEW_MODEL__


#include <QAbstractItemModel>
#include <QMap>
#include <QVector>

#include <memory>


class ChatUsersViewModel : public QAbstractTableModel
{
   Q_OBJECT

public:
   ChatUsersViewModel(QObject* parent = nullptr);
   ~ChatUsersViewModel() noexcept override = default;

   ChatUsersViewModel(const ChatUsersViewModel&) = delete;
   ChatUsersViewModel& operator = (const ChatUsersViewModel&) = delete;

   ChatUsersViewModel(ChatUsersViewModel&&) = delete;
   ChatUsersViewModel& operator = (ChatUsersViewModel&&) = delete;

   QString resolveUser(const QModelIndex& index);
   QModelIndex resolveUser(const QString& userId);

public:
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
   void onClear();
   void onUsersBeginUpdate(int count);
   void onUserUpdate(const QString& userId);
   void onUsersEndUpdate();

private:
   QMap<QString, int> indexByUser_;
   QVector<QString> userByIndex_;
};


#endif
