#ifndef CHATCLIENTUSERVIEW_H
#define CHATCLIENTUSERVIEW_H

#include <QTreeView>
#include <QLabel>
#include <QDebug>
#include "ChatHandleInterfaces.h"
#include "ChatUsersViewItemStyle.h"

namespace Chat {
    class MessageData;
   }

class ChatUsersContextMenu;
class ChatUsersViewItemStyle;
class ChatClientUserView : public QTreeView
{
   Q_OBJECT
public:
   ChatClientUserView(QWidget * parent = nullptr);
   void addWatcher(ViewItemWatcher* watcher);
   void setActiveChatLabel(QLabel * label);
   void setHandler(std::shared_ptr<ChatItemActionsHandler> handler);
   void setCurrentUserChat(const QString &userId);

public slots:
   void onCustomContextMenu(const QPoint &);
private slots:
   void onClicked(const QModelIndex &);
private:
   void updateDependUI(CategoryElement * element);
   void notifyCurrentChanged(CategoryElement *element);
   void notifyMessageChanged(std::shared_ptr<Chat::MessageData> message);
   void notifyElementUpdated(CategoryElement *element);
private:
   std::list<ViewItemWatcher* > watchers_;
   std::shared_ptr<ChatItemActionsHandler> handler_;
   QLabel * label_;
   ChatUsersContextMenu* contextMenu_;


   // QAbstractItemView interface
protected slots:
   void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

   // QAbstractItemView interface
protected slots:
   void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) override;

   // QAbstractItemView interface
protected slots:
   void rowsInserted(const QModelIndex &parent, int start, int end) override;
};







#endif // CHATCLIENTUSERVIEW_H
