#include "BlocksViewModel.h"

#include <QDateTime>
#include "ArmoryConnection.h"

namespace {

const int kLatestBlockCount = 10;

}

BlocksViewModel::BlocksViewModel(const std::shared_ptr<spdlog::logger> &logger
   ,const std::shared_ptr<ArmoryConnection> &armory, QObject *parent)
   : QAbstractTableModel(parent)
   , logger_(logger)
   , armory_(armory)
{
}

BlocksViewModel::~BlocksViewModel() noexcept = default;

int BlocksViewModel::columnCount(const QModelIndex&) const
{
   return int(Columns::TotalCount);
}

int BlocksViewModel::rowCount(const QModelIndex&) const
{
   return int(currentPage_.size());
}

QVariant BlocksViewModel::data(const QModelIndex &index, int role) const
{
   int row = index.row();
   if (role != Qt::DisplayRole || row < 0 || row >= int(currentPage_.size())) {
      return QVariant();
   }

   const ClientClasses::BlockHeader& item = currentPage_[row].blockHeader;

   switch (Columns(index.column())) {
   case Columns::Height:
      return item.getBlockHeight();
   case Columns::Age:
      return QDateTime::fromSecsSinceEpoch(item.getTimestamp());
   case Columns::Miner:
      // FIXME: Read coin base text and try to find miner identity
      return QVariant();
   case Columns::TxCount:
      // FIXME: Return correct transaction count
      return QLatin1String("1234");
   case Columns::Size:
      // FIXME:
      return QLatin1String("987.65");
   default: return tr("Unknown");
   }
}

QVariant BlocksViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (role != Qt::DisplayRole || orientation == Qt::Vertical) {
       return QVariant();
   }

   switch (Columns(section)) {
      case Columns::Height: return tr("Height");
      case Columns::Age: return tr("Age");
      case Columns::Miner: return tr("Miner");
      case Columns::TxCount: return tr("Transactions");
      case Columns::Size: return tr("Size (kB)");
      default: return tr("Unknown");
   }
}

void BlocksViewModel::updateFromTopBlocks()
{
   const unsigned topBlock = armory_->topBlock();
   if (topBlock == 0) {
      logger_->error("BlocksViewModel::updateFromBlock: topBlock == 0");
      return;
   }

   currentPageUpdated_.clear();
   currentPageUpdated_.resize(kLatestBlockCount);
   currentPageUpdatedCount_ = kLatestBlockCount;

   currentPageUpdatedReqId_ += 1;
   int reqId = currentPageUpdatedReqId_;

   for (int latestBlockIndex = 0; latestBlockIndex < kLatestBlockCount; ++latestBlockIndex) {
      armory_->getBlockHeaderByHeigh(int(topBlock) - latestBlockIndex, [this, latestBlockIndex, reqId](ClientClasses::BlockHeader blockHeader) {
         QMetaObject::invokeMethod(this, [this, latestBlockIndex, blockHeader, reqId]() {
            updateLatestBlock(latestBlockIndex, reqId, blockHeader);
         });
      });
   }
}

void BlocksViewModel::updateLatestBlock(int index, int reqId, const ClientClasses::BlockHeader &blockHeader)
{
   if (reqId != currentPageUpdatedReqId_) {
      return;
   }

   if (currentPageUpdatedCount_ <= 0) {
      logger_->debug("BlocksViewModel::updateLatestBlock: currentPageUpdatedCount_ <= 0");
      return;
   }

   if (index >= int(currentPageUpdated_.size())) {
      logger_->debug("BlocksViewModel::updateLatestBlock: index >= int(currentPageUpdated_.size()");
      return;
   }

   currentPageUpdatedCount_ -= 1;
   currentPageUpdated_[index].blockHeader = blockHeader;

   if (currentPageUpdatedCount_ == 0) {
      beginResetModel();
      currentPage_ = std::move(currentPageUpdated_);
      endResetModel();
   }
}
