#ifndef CHATCLIENTUSERVIEW_H
#define CHATCLIENTUSERVIEW_H

#include <QTreeView>
#include <QLabel>
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
   void setHandler(ChatItemActionsHandler * handler);
   void setCurrentUserChat(const std::string &userId);
   void updateCurrentChat();

public slots:
   void onCustomContextMenu(const QPoint &);
private slots:
   void onClicked(const QModelIndex &);
   void onDoubleClicked(const QModelIndex &);
private:
   void updateDependUI(CategoryElement * element);
   void notifyCurrentChanged(CategoryElement *element);
   void notifyMessageChanged(std::shared_ptr<Chat::Data> message);
   void notifyElementUpdated(CategoryElement *element);
   void notifyCurrentAboutToBeRemoved();
   void editContact(std::shared_ptr<Chat::Data> crecord);
private:
   friend ChatUsersContextMenu;
   std::list<ViewItemWatcher* > watchers_;
   ChatItemActionsHandler * handler_;
   QLabel * label_;
   ChatUsersContextMenu* contextMenu_;


   // QAbstractItemView interface
protected slots:
   void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;
   void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) override;
   void rowsInserted(const QModelIndex &parent, int start, int end) override;
   void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) override;
};
#endif // CHATCLIENTUSERVIEW_H
