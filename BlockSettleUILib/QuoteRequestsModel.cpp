#include <chrono>
#include "QuoteRequestsModel.h"
#include "AssetManager.h"
#include "CelerSubmitQuoteNotifSequence.h"
#include "CurrencyPair.h"
#include "QuoteRequestsWidget.h"
#include "SettlementContainer.h"
#include "UiUtils.h"


QString QuoteRequestsModel::Header::toString(QuoteRequestsModel::Header::Index h)
{
   switch (h) {
      case SecurityID:  return tr("SecurityID");
      case Product:     return tr("Product");
      case Side:        return tr("Side");
      case Quantity:    return tr("Quantity");
      case Party:       return tr("Party");
      case Status:      return tr("Status");
      case QuotedPx:    return tr("Quoted Price");
      case IndicPx:     return tr("Indicative Px");
      case BestPx:      return tr("Best Quoted Px");
      case Empty:       return QString();
      default:          return tr("Undefined");
   }
}

QStringList QuoteRequestsModel::Header::labels()
{
   QStringList rv;
   for (int i = Header::first; i < Header::last; i++) {
      rv << Header::toString(static_cast<Header::Index>(i));
   }
   return rv;
}


QuoteRequestsModel::QuoteRequestsModel(const std::shared_ptr<bs::SecurityStatsCollector> &statsCollector
 , QObject* parent)
   : QStandardItemModel(parent)
   , secStatsCollector_(statsCollector)
{
   setHorizontalHeaderLabels(Header::labels());

   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, this, &QuoteRequestsModel::ticker);
   timer_.start();
}

QuoteRequestsModel::~QuoteRequestsModel()
{
   secStatsCollector_->saveState();
}

void QuoteRequestsModel::SetAssetManager(const std::shared_ptr<AssetManager>& assetManager)
{
   assetManager_ = assetManager;
}

void QuoteRequestsModel::ticker() {
   std::unordered_set<std::string>  deletedRows;
   const auto timeNow = QDateTime::currentDateTime();

   for (const auto &id : pendingDeleteIds_) {
      forSpecificId(id, [](QStandardItem *grp, int idxItem) {
         grp->removeRow(idxItem);
      });
      deletedRows.insert(id);
   }
   pendingDeleteIds_.clear();

   for (auto qrn : notifications_) {
      const auto timeDiff = timeNow.msecsTo(qrn.second.expirationTime.addMSecs(qrn.second.timeSkewMs));
      if ((timeDiff < 0) || (qrn.second.status == bs::network::QuoteReqNotification::Withdrawn)) {
         forSpecificId(qrn.second.quoteRequestId, [](QStandardItem *grp, int itemIndex) {
            grp->removeRow(itemIndex);
            if ((grp->rowCount() == 0) && grp->parent()) {
               grp->parent()->removeRow(grp->row());
            }
         });
         deletedRows.insert(qrn.second.quoteRequestId);
      }
      else if ((qrn.second.status == bs::network::QuoteReqNotification::PendingAck)
         || (qrn.second.status == bs::network::QuoteReqNotification::Replied)) {
         forSpecificId(qrn.second.quoteRequestId, [timeDiff](QStandardItem *grp, int itemIndex) {
            grp->child(itemIndex, Header::Status)->setData((int)timeDiff, Role::TimeLeft);
            grp->child(itemIndex)->setData((int)timeDiff, Role::TimeLeft);
         });
      }
   }

   for (auto delRow : deletedRows) {
      notifications_.erase(delRow);
   }

   for (const auto &settlContainer : settlContainers_) {
      forSpecificId(settlContainer.second->id(),
         [timeLeft = settlContainer.second->timeLeftMs()](QStandardItem *grp, int itemIndex) {
         grp->child(itemIndex, Header::Status)->setData(timeLeft, Role::TimeLeft);
         grp->child(itemIndex)->setData(timeLeft, Role::TimeLeft);
      });
   }
}

void QuoteRequestsModel::onQuoteNotifCancelled(const QString &reqId)
{
   forSpecificId(reqId.toStdString(), [](QStandardItem *group, int i) {
      group->child(i, Header::QuotedPx)->setText(tr("pulled"));
   });

   setStatus(reqId.toStdString(), bs::network::QuoteReqNotification::PendingAck);
}

void QuoteRequestsModel::onQuoteReqCancelled(const QString &reqId, bool byUser)
{
   if (!byUser) {
      return;
   }
   setStatus(reqId.toStdString(), bs::network::QuoteReqNotification::Withdrawn);
}

void QuoteRequestsModel::onQuoteRejected(const QString &reqId, const QString &reason)
{
   setStatus(reqId.toStdString(), bs::network::QuoteReqNotification::Rejected, reason);
}

void QuoteRequestsModel::onBestQuotePrice(const QString reqId, double price, bool own)
{
   forSpecificId(reqId.toStdString(), [price, own](QStandardItem *grp, int index) {
      const auto assetType = static_cast<bs::network::Asset::Type>(grp->child(index, Header::SecurityID)->data(Role::AssetType).toInt());

      grp->child(index, Header::BestPx)->setText(UiUtils::displayPriceForAssetType(price, assetType));
      grp->child(index)->setData(price, Role::BestQPrice);
      grp->child(index, Header::QuotedPx)->setBackground(
         colorForQuotedPrice(grp->child(index)->data(Role::QuotedPrice).toDouble(), price, own));
   });
}

void QuoteRequestsModel::onQuoteReqNotifReplied(const bs::network::QuoteNotification &qn)
{
   forSpecificId(qn.quoteRequestId, [&qn](QStandardItem *group, int i) {
      const auto assetType = static_cast<bs::network::Asset::Type>(group->child(i, Header::SecurityID)->data(Role::AssetType).toInt());
      const double quotedPrice = (qn.side == bs::network::Side::Buy) ? qn.bidPx : qn.offerPx;

      group->child(i, Header::QuotedPx)->setText(UiUtils::displayPriceForAssetType(quotedPrice, assetType));
      group->child(i)->setData(quotedPrice, Role::QuotedPrice);
      group->child(i, Header::QuotedPx)->setBackground(
         colorForQuotedPrice(quotedPrice, group->child(i)->data(Role::BestQPrice).toDouble()));
   });

   setStatus(qn.quoteRequestId, bs::network::QuoteReqNotification::Replied);
}

QBrush QuoteRequestsModel::colorForQuotedPrice(double quotedPrice, double bestQPrice, bool own)
{
   if (qFuzzyIsNull(quotedPrice) || qFuzzyIsNull(bestQPrice)) {
      return {};
   }
   if (own && qFuzzyCompare(quotedPrice, bestQPrice)) {
      return Qt::darkGreen;
   }
   return Qt::darkRed;
}

void QuoteRequestsModel::onQuoteReqNotifReceived(const bs::network::QuoteReqNotification &qrn)
{
   QString groupNameType = tr(bs::network::Asset::toString(qrn.assetType));
   auto groupItemsType = findItems(groupNameType);
   QStandardItem* groupItemType = nullptr;
   if (groupItemsType.isEmpty()) {
      groupItemType = new QStandardItem(groupNameType);
      appendRow(QList<QStandardItem*>() << groupItemType);
   }
   else {
      groupItemType = groupItemsType.first();
   }

   QString groupNameSec = QString::fromStdString(qrn.security);
   QStandardItem* groupItemSec = nullptr;
   int indexSec = -1;
   for (int i = 0; i < groupItemType->rowCount(); i++) {
      if (groupItemType->child(i, 0)->text() == groupNameSec) {
         indexSec = i;
         break;
      }
   }

   if (indexSec < 0) {
      groupItemSec = new QuoteReqItem(secStatsCollector_, groupNameSec, groupNameSec);
      QFont font;
      font.setBold(true);
      groupItemSec->setData(font, Qt::FontRole);
      groupItemSec->setData(true, Role::AllowFiltering);
      groupItemType->appendRow(QList<QStandardItem*>() << groupItemSec);
   }
   else {
      groupItemSec = groupItemType->child(indexSec, 0);
   }

   updateRow(groupItemSec, qrn);
}

void QuoteRequestsModel::updateRow(QStandardItem *group, const bs::network::QuoteReqNotification &qrn)
{
   auto itQRN = notifications_.find(qrn.quoteRequestId);

   if (itQRN == notifications_.end()) {
      const auto assetType = assetManager_->GetAssetTypeForSecurity(qrn.security);

      QList<QStandardItem*> columns;
      const QString security = QString::fromStdString(qrn.security);
      QStandardItem *siSecurity = new QuoteReqItem(secStatsCollector_, security, security);
      siSecurity->setData(QString::fromStdString(qrn.quoteRequestId), Role::ReqId);
      siSecurity->setData(static_cast<int>(assetType), Role::AssetType);
      siSecurity->setData(QString::fromStdString(qrn.product), Role::Product);
      siSecurity->setData(true, Role::AllowFiltering);
      columns << siSecurity;

      columns << new QuoteReqItem(secStatsCollector_, QString::fromStdString(qrn.product), security);

      auto siSide = new QuoteReqItem(secStatsCollector_, tr(bs::network::Side::toString(qrn.side)), security);
      siSide->setData(static_cast<int>(qrn.side), Role::Side);

      const auto amountStr = (qrn.assetType == bs::network::Asset::Type::PrivateMarket) ? UiUtils::displayCCAmount(qrn.quantity)
         : UiUtils::displayQty(qrn.quantity, qrn.product);
      auto siQuantity = new QuoteReqItem(secStatsCollector_, amountStr, security);
      siQuantity->setTextAlignment(Qt::AlignRight);

      columns << siSide << siQuantity << new QuoteReqItem(secStatsCollector_, QString(), security);

      auto siStatus = new QStandardItem(quoteReqStatusDesc(qrn.status));
      const bool showProgress = ((qrn.status == bs::network::QuoteReqNotification::Status::PendingAck)
         || (qrn.status == bs::network::QuoteReqNotification::Status::Replied));
      siStatus->setData(showProgress, Role::ShowProgress);
      siStatus->setData(30000, Role::Timeout);     // QRN timeout in ms
      columns << siStatus;

      std::vector<QStandardItem *> priceItems = { new QStandardItem() };   // Quote price

      const CurrencyPair cp(qrn.security);
      const bool isBid = (qrn.side == bs::network::Side::Buy) ^ (cp.NumCurrency() == qrn.product);
      const double indicPrice = isBid ? mdPrices_[qrn.security][Role::BidPrice] : mdPrices_[qrn.security][Role::OfferPrice];
      if (!qFuzzyIsNull(indicPrice)) {
         priceItems.push_back(new QStandardItem(UiUtils::displayPriceForAssetType(indicPrice, assetType)));
      }
      else {
         priceItems.push_back(new QStandardItem());
      }

      priceItems.push_back(new QStandardItem());   // Best quoted price

      for (auto priceItem : priceItems) {
         priceItem->setTextAlignment(Qt::AlignRight);
         columns << priceItem;
      }

      group->appendRow(columns);
      notifications_[qrn.quoteRequestId] = qrn;
   }
   else {
      setStatus(qrn.quoteRequestId, qrn.status);
   }
}

void QuoteRequestsModel::addSettlementContainer(const std::shared_ptr<bs::SettlementContainer> &container)
{
   settlContainers_[container->id()] = container;
   connect(container.get(), &bs::SettlementContainer::failed, this, &QuoteRequestsModel::onSettlementFailed);
   connect(container.get(), &bs::SettlementContainer::completed, this, &QuoteRequestsModel::onSettlementCompleted);
   connect(container.get(), &bs::SettlementContainer::timerExpired, this, &QuoteRequestsModel::onSettlementExpired);

   QStandardItem* groupItem = nullptr;
   auto groupItems = findItems(groupNameSettlements_);
   if (groupItems.isEmpty()) {
      groupItem = new QStandardItem(groupNameSettlements_);
      QFont font;
      font.setBold(true);
      groupItem->setData(font, Qt::FontRole);
      groupItem->setData(1, Role::Grade);

      auto siCompleted = new QStandardItem();
      siCompleted->setData(QColor(Qt::green), Qt::TextColorRole);
      siCompleted->setTextAlignment(Qt::AlignRight);

      auto siFailed = new QStandardItem();
      siFailed->setData(QColor(Qt::red), Qt::TextColorRole);
      siFailed->setTextAlignment(Qt::AlignRight);

      QList<QStandardItem*> rootRow;
      rootRow << groupItem << siCompleted << siFailed;
      appendRow(rootRow);
   }
   else {
      groupItem = groupItems.first();
   }

   const auto collector = std::make_shared<bs::SettlementStatsCollector>(container);
   const auto assetType = assetManager_->GetAssetTypeForSecurity(container->security());
   QList<QStandardItem*> columns;
   QStandardItem *siSecurity = new QuoteReqItem(collector, QString::fromStdString(container->security()));
   siSecurity->setData(static_cast<int>(assetType), Role::AssetType);
   siSecurity->setData(QString::fromStdString(container->id()), Role::ReqId);
   siSecurity->setData(QString::fromStdString(container->product()), Role::Product);
   columns << siSecurity;

   columns << new QuoteReqItem(collector, QString::fromStdString(container->product()));

   auto siSide = new QuoteReqItem(collector, tr(bs::network::Side::toString(container->side())));
   siSide->setData(static_cast<int>(container->side()), Role::Side);

   const auto amountStr = (container->assetType() == bs::network::Asset::Type::PrivateMarket)
      ? UiUtils::displayCCAmount(container->quantity()) : UiUtils::displayQty(container->quantity(), container->product());
   auto siQuantity = new QuoteReqItem(collector, amountStr);
   siQuantity->setTextAlignment(Qt::AlignRight);

   columns << siSide << siQuantity << new QuoteReqItem(collector, {});

   auto siStatus = new QStandardItem();
   connect(container.get(), &bs::SettlementContainer::timerStarted, [siStatus](int msDuration) {
      siStatus->setData(msDuration, Role::Timeout);
   });
   siStatus->setData(true, Role::ShowProgress);
   columns << siStatus;

   std::vector<QStandardItem *> priceItems = { new QuoteReqItem(collector, UiUtils::displayPriceForAssetType(container->price(), assetType))
      , new QStandardItem(), new QStandardItem() };

   for (auto priceItem : priceItems) {
      priceItem->setTextAlignment(Qt::AlignRight);
      columns << priceItem;
   }

   groupItem->appendRow(columns);
}


void QuoteRequestsModel::onSettlementCompleted()
{
   settlCompleted_++;
   deleteSettlement(qobject_cast<bs::SettlementContainer *>(sender()));
}

void QuoteRequestsModel::onSettlementFailed()
{
   settlFailed_++;
   deleteSettlement(qobject_cast<bs::SettlementContainer *>(sender()));
}

void QuoteRequestsModel::onSettlementExpired()
{
   deleteSettlement(qobject_cast<bs::SettlementContainer *>(sender()));
}

void QuoteRequestsModel::deleteSettlement(bs::SettlementContainer *container)
{
   updateSettlementCounters();
   if (container) {
      pendingDeleteIds_.insert(container->id());
      container->deactivate();
      settlContainers_.erase(container->id());
   }
}

void QuoteRequestsModel::updateSettlementCounters()
{
   for (int i = 0; i < rowCount(); i++) {
      if (item(i, 0)->text() == groupNameSettlements_) {
         if (settlCompleted_) {
            item(i, 1)->setText(QString::number(settlCompleted_));
         }
         if (settlFailed_) {
            item(i, 2)->setText(QString::number(settlFailed_));
         }
         break;
      }
   }
}

void QuoteRequestsModel::forSpecificId(const std::string &reqId, const cbItem &cb)
{
   const auto id = QString::fromStdString(reqId);
   for (int i = 0; i < rowCount(); i++) {
      auto groupItemType = item(i);
      for (int j = 0; j < groupItemType->rowCount(); j++) {
         QStandardItem *groupItemSec = groupItemType->child(j, 0);
         if (groupItemSec->data(Role::ReqId).toString() == id) {
            cb(groupItemType, j);
            return;
         }
         for (int k = 0; k < groupItemSec->rowCount(); k++) {
            QStandardItem *item = groupItemSec->child(k, 0);
            if (item->data(Role::ReqId).toString() == id) {
               cb(groupItemSec, k);
               return;
            }
         }
      }
   }
}

void QuoteRequestsModel::forEachSecurity(const QString &security, const cbItem &cb)
{
   for (int i = 0; i < rowCount(); i++) {
      auto groupItemType = item(i);
      for (int j = 0; j < groupItemType->rowCount(); j++) {
         QStandardItem *groupItemSec = groupItemType->child(j, 0);
         if (groupItemSec->text() != security)
            continue;
         for (int k = 0; k < groupItemSec->rowCount(); k++) {
            cb(groupItemSec, k);
         }
      }
   }
}

const bs::network::QuoteReqNotification &QuoteRequestsModel::getQuoteReqNotification(const std::string &id) const
{
   static bs::network::QuoteReqNotification   emptyQRN;

   auto itQRN = notifications_.find(id);
   if (itQRN == notifications_.end())  return emptyQRN;
   return itQRN->second;
}

double QuoteRequestsModel::getPrice(const std::string &security, Role::Index role) const
{
   const auto itMDP = mdPrices_.find(security);
   if (itMDP != mdPrices_.end()) {
      const auto itP = itMDP->second.find(role);
      if (itP != itMDP->second.end()) {
         return itP->second;
      }
   }
   return 0;
}

QString QuoteRequestsModel::quoteReqStatusDesc(bs::network::QuoteReqNotification::Status status)
{
   switch (status) {
      case bs::network::QuoteReqNotification::Withdrawn:    return tr("Withdrawn");
      case bs::network::QuoteReqNotification::PendingAck:   return tr("PendingAck");
      case bs::network::QuoteReqNotification::Replied:      return tr("Replied");
      case bs::network::QuoteReqNotification::TimedOut:     return tr("TimedOut");
      case bs::network::QuoteReqNotification::Rejected:     return tr("Rejected");
      default:       return QString();
   }
}

QBrush QuoteRequestsModel::bgColorForStatus(bs::network::QuoteReqNotification::Status status)
{
   switch (status) {
      case bs::network::QuoteReqNotification::Withdrawn:    return Qt::magenta;
      case bs::network::QuoteReqNotification::Rejected:     return Qt::red;
      case bs::network::QuoteReqNotification::Replied:      return Qt::green;
      case bs::network::QuoteReqNotification::TimedOut:     return Qt::yellow;
      case bs::network::QuoteReqNotification::PendingAck:
      default:       return QBrush();
   }
}

void QuoteRequestsModel::setStatus(const std::string &reqId, bs::network::QuoteReqNotification::Status status
   , const QString &details)
{
   auto itQRN = notifications_.find(reqId);
   if (itQRN != notifications_.end()) {
      itQRN->second.status = status;
      forSpecificId(reqId, [status, details](QStandardItem *grp, int index) {
         if (!details.isEmpty()) {
            grp->child(index, Header::Status)->setText(details);
         }
         else {
            grp->child(index, Header::Status)->setText(quoteReqStatusDesc(status));
         }
         grp->child(index, Header::Status)->setBackground(bgColorForStatus(status));

         const bool showProgress = ((status == bs::network::QuoteReqNotification::Status::PendingAck)
            || (status == bs::network::QuoteReqNotification::Status::Replied));
         grp->child(index, Header::Status)->setData(showProgress, Role::ShowProgress);
      });
      emit quoteReqNotifStatusChanged(itQRN->second);
   }
}

void QuoteRequestsModel::onSecurityMDUpdated(const QString &security, const bs::network::MDFields &mdFields)
{
   const auto pxBid = bs::network::MDField::get(mdFields, bs::network::MDField::PriceBid);
   const auto pxOffer = bs::network::MDField::get(mdFields, bs::network::MDField::PriceOffer);
   if (pxBid.type != bs::network::MDField::Unknown) {
      mdPrices_[security.toStdString()][Role::BidPrice] = pxBid.value;
   }
   if (pxOffer.type != bs::network::MDField::Unknown) {
      mdPrices_[security.toStdString()][Role::OfferPrice] = pxOffer.value;
   }

   forEachSecurity(security, [security, pxBid, pxOffer, this](QStandardItem *grp, int index) {
      const CurrencyPair cp(security.toStdString());
      const bool isBuy = (static_cast<bs::network::Side::Type>(grp->child(index, Header::Side)->data(Role::Side).toInt()) == bs::network::Side::Buy)
         ^ (cp.NumCurrency() == grp->child(index, Header::first)->data(Role::Product).toString().toStdString());
      double indicPrice = 0;
      if (isBuy && (pxBid.type != bs::network::MDField::Unknown)) {
         indicPrice = pxBid.value;
      }
      else if (!isBuy && (pxOffer.type != bs::network::MDField::Unknown)) {
         indicPrice = pxOffer.value;
      }
      if (indicPrice > 0) {
         const auto prevPrice = grp->child(index, Header::IndicPx)->data(Qt::DisplayRole).toDouble();
         const auto assetType = static_cast<bs::network::Asset::Type>(grp->child(index, Header::SecurityID)->data(Role::AssetType).toInt());
         grp->child(index, Header::IndicPx)->setText(UiUtils::displayPriceForAssetType(indicPrice, assetType));

         if (!qFuzzyIsNull(prevPrice)) {
            if (indicPrice > prevPrice) {
               grp->child(index, Header::IndicPx)->setBackground(Qt::darkGreen);
            }
            else if (indicPrice < prevPrice) {
               grp->child(index, Header::IndicPx)->setBackground(Qt::darkRed);
            }
         }
      }
   });
}


QuoteReqItem::QuoteReqItem(const std::shared_ptr<bs::StatsCollector> &sc, const QString &text, const QString &key)
   : QStandardItem(text), statsCollector_(sc), key_(key)
{}

QVariant QuoteReqItem::data(int role) const
{
   if (statsCollector_) {
      switch (role)
      {
      case Qt::TextColorRole:
         return statsCollector_->getColorFor(key_.toStdString());

      case QuoteRequestsModel::Role::Grade:
         return statsCollector_->getGradeFor(key_.toStdString());

      default: break;
      }
   }
   return QStandardItem::data(role);
}
