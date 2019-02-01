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

   QString resolveUser(const QModelIndex &) const;

   bool isUserInModel(const std::string &userId) const;

public:
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
   void onUsersReplace(const std::vector<std::string> &);
   void onUsersAdd(const std::vector<std::string> &);
   void onUsersDel(const std::vector<std::string> &);

private:
   std::vector<std::string>   users_;
};


#endif
