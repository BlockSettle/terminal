#ifndef __BLOCKS_VIEW_MODEL_H__
#define __BLOCKS_VIEW_MODEL_H__

#include <memory>
#include <vector>
#include <QAbstractItemModel>
#include "ClientClasses.h"

class ArmoryConnection;

namespace spdlog {
class logger;
}

struct BlocksViewItem
{
   ClientClasses::BlockHeader blockHeader;
};
using BlocksViewItems = std::vector<BlocksViewItem>;

Q_DECLARE_METATYPE(BlocksViewItem)
Q_DECLARE_METATYPE(BlocksViewItems)


class BlocksViewModel : public QAbstractTableModel
{
Q_OBJECT

public:
    BlocksViewModel(const std::shared_ptr<spdlog::logger> &logger
       , const std::shared_ptr<ArmoryConnection> &armory
       , QObject *parent = nullptr);
   ~BlocksViewModel() noexcept override;

   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

   void updateFromTopBlocks();

private:
   void updateLatestBlock(int index, int reqId, const ClientClasses::BlockHeader& blockHeader);

   enum class Columns
   {
      Height,
      Age,
      Miner,
      TxCount,
      Size,

      TotalCount
   };

   BlocksViewItems currentPage_;
   BlocksViewItems currentPageUpdated_;
   int currentPageUpdatedCount_{0};
   int currentPageUpdatedReqId_{0};
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<ArmoryConnection> armory_;
};

#endif // __BLOCKS_VIEW_MODEL_H__
