#ifndef CHATHANDLEINTERFACES_H
#define CHATHANDLEINTERFACES_H
#include <memory>
#include <map>
#include <QString>
#include "chat.pb.h"

namespace Chat {
   class MessageData;
}

class CategoryElement;
class ViewItemWatcher {
public:
   virtual void onElementSelected(CategoryElement* element) = 0;
   virtual void onElementUpdated(CategoryElement* element) = 0;
   virtual void onMessageChanged(std::shared_ptr<Chat::Data> message) = 0;
   virtual void onCurrentElementAboutToBeRemoved() = 0;
   virtual ~ViewItemWatcher() = default;
};

class ChatItemActionsHandler {
public:
   virtual ~ChatItemActionsHandler() = default;
   virtual void onActionCreatePendingOutgoing(const std::string& userId) = 0;
   virtual void onActionRemoveFromContacts(std::shared_ptr<Chat::Data> crecord) = 0;
   virtual void onActionAcceptContactRequest(std::shared_ptr<Chat::Data> crecord) = 0;
   virtual void onActionRejectContactRequest(std::shared_ptr<Chat::Data> crecord) = 0;
   virtual void onActionEditContactRequest(std::shared_ptr<Chat::Data> crecord) = 0;
   virtual bool onActionIsFriend(const std::string& userId) = 0;
};

class ChatSearchActionsHandler {
public:
   virtual ~ChatSearchActionsHandler() = default;
   virtual void onActionSearchUsers(const std::string& pattern) = 0;
   virtual void onActionResetSearch() = 0;
};

class ChatMessageReadHandler {
public:
   virtual ~ChatMessageReadHandler() = default;
   virtual void onMessageRead(std::shared_ptr<Chat::Data> message) = 0;
   virtual void onRoomMessageRead(std::shared_ptr<Chat::Data> message) = 0;
};

class NewMessageMonitor {
public:

   virtual  ~NewMessageMonitor() = default;
   virtual void onNewMessagesPresent(std::map<std::string, std::shared_ptr<Chat::Data>> newMessages) = 0;
};
class ModelChangesHandler {
public:

   virtual  ~ModelChangesHandler() = default;
   virtual void onContactUpdatedByInput(std::shared_ptr<Chat::Data> crecord) = 0;
};


#endif // CHATHANDLEINTERFACES_H
