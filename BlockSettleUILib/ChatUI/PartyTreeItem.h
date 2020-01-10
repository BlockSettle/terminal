/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef PARTYTREEITEM_H
#define PARTYTREEITEM_H

#include <QVariant>
#include "QList"

#include "../BlocksettleNetworkingLib/ChatProtocol/Party.h"
#include "chat.pb.h"
// Internal enum

namespace bs {
   namespace network {
      namespace otc {
         enum class PeerType : int;
      }
   }
}

namespace UI {
   enum class ElementType
   {
      Root = 0,
      Container,
      Party,
   };
}

struct ReusableItemData
{
   int unseenCount_{};
   bool otcTogglingMode_{};
};

class PartyTreeItem
{
public:
   PartyTreeItem(const QVariant& data, UI::ElementType modelType, PartyTreeItem* parent = nullptr);
   ~PartyTreeItem();

   PartyTreeItem* child(int number);

   int childCount() const;
   int columnCount() const;

   QVariant data() const;

   bool insertChildren(std::unique_ptr<PartyTreeItem>&& item);
   PartyTreeItem* parent();
   void removeAll();
   int childNumber() const;
   bool setData(const QVariant& value);

   UI::ElementType modelType() const;

   void increaseUnseenCounter(int newMessageCount);
   void decreaseUnseenCounter(int seenMessageCount);
   bool hasNewMessages() const;
   int unseenCount() const;

   void enableOTCToggling(bool otcToggling);
   bool isOTCTogglingMode() const;

   void changeOTCToggleState();
   bool activeOTCToggleState() const;

   void applyReusableData(const ReusableItemData& data);
   ReusableItemData generateReusableData() const;

   bs::network::otc::PeerType peerType{};

private:
   std::vector<std::unique_ptr<PartyTreeItem>> childItems_;
   QVariant itemData_;
   PartyTreeItem* parentItem_;
   UI::ElementType modelType_;
   int unseenCounter_{};

   // OTC toggling
   bool otcTogglingMode_{};
   bool currentOTCToggleState_{};
};
#endif // PARTYTREEITEM_H
