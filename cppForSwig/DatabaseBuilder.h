////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "BlockDataMap.h"
#include "Blockchain.h"
#include "bdmenums.h"
#include "Progress.h"

class BlockDataManager;
class ScrAddrFilter;
class UnresolvedHashException {};

typedef std::function<void(BDMPhase, double, unsigned, unsigned)> ProgressCallback;

/////////////////////////////////////////////////////////////////////////////
class DatabaseBuilder
{
private:
   BlockFiles& blockFiles_;
   std::shared_ptr<Blockchain> blockchain_;
   LMDBBlockDatabase* db_;
   std::shared_ptr<ScrAddrFilter> scrAddrFilter_;

   const ProgressCallback progress_;
   BlockOffset topBlockOffset_;
   const BlockDataManagerConfig bdmConfig_;

   unsigned checkedTransactions_ = 0;
   const bool forceRescanSSH_;

private:
   BlockOffset loadBlockHeadersFromDB(const ProgressCallback &progress);
   
   bool addBlocksToDB(
      BlockDataLoader& bdl, uint16_t fileID, size_t startOffset,
      std::shared_ptr<BlockOffset> bo, bool fullHints);
   void parseBlockFile(const uint8_t* fileMap, size_t fileSize, size_t startOffset,
      std::function<bool(const uint8_t* data, size_t size, size_t offset)>);

   Blockchain::ReorganizationState updateBlocksInDB(
      const ProgressCallback &progress, bool verbose, bool fullHints);
   BinaryData initTransactionHistory(int32_t startHeight);
   BinaryData scanHistory(int32_t startHeight, bool reportprogress, bool init);
   void undoHistory(Blockchain::ReorganizationState& reorgState);

   void resetHistory(void);
   bool reparseBlkFiles(unsigned fromID);
   std::map<BinaryData, std::shared_ptr<BlockHeader>> assessBlkFile(BlockDataLoader& bdl,
      unsigned fileID);

   void verifyTransactions(void);
   void commitAllTxHints(
      const std::map<uint32_t, BlockData>&, const std::set<unsigned>&);
   void commitAllStxos(std::shared_ptr<BlockDataFileMap>,
      const std::map<uint32_t, BlockData>&, const std::set<unsigned>&);

   void repairTxFilters(const std::set<unsigned>&);
   void reprocessTxFilter(std::shared_ptr<BlockDataFileMap>, unsigned);

   void cycleDatabases(void);

public:
   DatabaseBuilder(BlockFiles&, BlockDataManager&,
      const ProgressCallback&, bool);

   void init(void);
   Blockchain::ReorganizationState update(void);

   void verifyChain(void);
   unsigned getCheckedTxCount(void) const { return checkedTransactions_; }

   void verifyTxFilters(void);
};
