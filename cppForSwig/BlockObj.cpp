////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2011-2015, Armory Technologies, Inc.                        //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE-ATI or http://www.gnu.org/licenses/agpl.html                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <thread>

#include "BinaryData.h"
#include "BtcUtils.h"
#include "BlockObj.h"
#include "lmdb_wrapper.h"

using namespace std;


////////////////////////////////////////////////////////////////////////////////
void BlockHeader::unserialize(uint8_t const * ptr, uint32_t size)
{
   if (size < HEADER_SIZE)
      throw BlockDeserializingException();
   dataCopy_.copyFrom(ptr, HEADER_SIZE);
   BtcUtils::getHash256(dataCopy_.getPtr(), HEADER_SIZE, thisHash_);
   difficultyDbl_ = BtcUtils::convertDiffBitsToDouble( 
                              BinaryDataRef(dataCopy_.getPtr()+72, 4));
   isInitialized_ = true;
   nextHash_ = BinaryData(0);
   blockHeight_ = UINT32_MAX;
   difficultySum_ = -1;
   isMainBranch_ = false;
   isOrphan_ = true;
   numTx_ = UINT32_MAX;
}

////////////////////////////////////////////////////////////////////////////////
void BlockHeader::unserialize(BinaryDataRef const & str) 
{ 
   unserialize(str.getPtr(), str.getSize()); 
}

////////////////////////////////////////////////////////////////////////////////
void BlockHeader::unserialize(BinaryRefReader & brr) 
{ 
   unserialize(brr.get_BinaryDataRef(HEADER_SIZE)); 
}




/////////////////////////////////////////////////////////////////////////////
void BlockHeader::pprint(ostream & os, int nIndent, bool pBigendian) const
{
   string indent = "";
   for(int i=0; i<nIndent; i++)
      indent = indent + "   ";

   string endstr = (pBigendian ? " (BE)" : " (LE)");
   os << indent << "Block Information: " << blockHeight_ << endl;
   os << indent << "   Hash:       " 
                << getThisHash().toHexStr(pBigendian).c_str() << endstr << endl;
   os << indent << "   Timestamp:  " << getTimestamp() << endl;
   os << indent << "   Prev Hash:  " 
                << getPrevHash().toHexStr(pBigendian).c_str() << endstr << endl;
   os << indent << "   MerkleRoot: " 
                << getMerkleRoot().toHexStr(pBigendian).c_str() << endstr << endl;
   os << indent << "   Difficulty: " << (difficultyDbl_)
                         << "    (" << getDiffBits().toHexStr().c_str() << ")" << endl;
   os << indent << "   CumulDiff:  " << (difficultySum_) << endl;
   os << indent << "   Nonce:      " << getNonce() << endl;
}

////////////////////////////////////////////////////////////////////////////////
void BlockHeader::pprintAlot(ostream & os)
{
   cout << "Header:   " << getBlockHeight() << endl;
   cout << "Hash:     " << getThisHash().toHexStr(true)  << endl;
   cout << "Hash:     " << getThisHash().toHexStr(false) << endl;
   cout << "PrvHash:  " << getPrevHash().toHexStr(true)  << endl;
   cout << "PrvHash:  " << getPrevHash().toHexStr(false) << endl;
   cout << "this*:    " << this << endl;
   cout << "TotSize:  " << getBlockSize() << endl;
   cout << "Tx Count: " << numTx_ << endl;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// DBOutPoint Methods
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BinaryDataRef DBOutPoint::getDBkey() const
{
   if (DBkey_.getSize() == 8)
      return DBkey_;

   if (db_ != nullptr)
   {
      DBkey_ = move(db_->getDBKeyForHash(txHash_));
      if (DBkey_.getSize() == 6)
      {
         DBkey_.append(WRITE_UINT16_BE((uint16_t)txOutIndex_));
         return DBkey_;
      }
   }

   return BinaryDataRef();
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// TxRef methods
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uint32_t TxRef::getBlockHeight(void) const
{
   if (dbKey6B_.getSize() == 6 &&
      !dbKey6B_.startsWith(DBUtils::ZeroConfHeader_))
      return DBUtils::hgtxToHeight(dbKey6B_.getSliceCopy(0, 4));
   else
      return UINT32_MAX;
}

/////////////////////////////////////////////////////////////////////////////
uint8_t TxRef::getDuplicateID(void) const
{
   if (dbKey6B_.getSize() == 6)
      return DBUtils::hgtxToDupID(dbKey6B_.getSliceCopy(0, 4));
   else
      return UINT8_MAX;
}

/////////////////////////////////////////////////////////////////////////////
uint16_t TxRef::getBlockTxIndex(void) const
{
   if (dbKey6B_.getSize() == 6)
   {
      if (!dbKey6B_.startsWith(DBUtils::ZeroConfHeader_))
         return READ_UINT16_BE(dbKey6B_.getPtr() + 4);
      else
         return READ_UINT32_BE(dbKey6B_.getPtr() + 2);
   }
   else
      return UINT16_MAX;
}


/////////////////////////////////////////////////////////////////////////////
void TxRef::pprint(ostream & os, int nIndent) const
{
   os << "TxRef Information:" << endl;
   //os << "   Hash:      " << getThisHash().toHexStr() << endl;
   os << "   Height:    " << getBlockHeight() << endl;
   os << "   BlkIndex:  " << getBlockTxIndex() << endl;
   //os << "   FileIdx:   " << blkFilePtr_.getFileIndex() << endl;
   //os << "   FileStart: " << blkFilePtr_.getStartByte() << endl;
   //os << "   NumBytes:  " << blkFilePtr_.getNumBytes() << endl;
   os << "   ----- " << endl;
   os << "   Read from disk, full tx-info: " << endl;
   //getTxCopy().pprint(os, nIndent+1); 
}

/////////////////////////////////////////////////////////////////////////////
void TxRef::setRef(BinaryDataRef bdr)
{
   dbKey6B_ = bdr.copy();
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// DBTxRef Methods
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
BinaryData DBTxRef::serialize(void) const 
{ 
   return db_->getFullTxCopy(dbKey6B_).serialize();
}

/////////////////////////////////////////////////////////////////////////////
Tx DBTxRef::getTxCopy(void) const
{
   return db_->getFullTxCopy(dbKey6B_);
}

/////////////////////////////////////////////////////////////////////////////
bool DBTxRef::isMainBranch(void) const
{
   if(dbKey6B_.getSize() != 6)
      return false;
   else
   {
      uint8_t dup8 = db_->getValidDupIDForHeight(getBlockHeight());
      return (getDuplicateID() == dup8);
   }
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBTxRef::getThisHash(void) const
{
   return db_->getTxHashForLdbKey(dbKey6B_);
}

/////////////////////////////////////////////////////////////////////////////
uint32_t DBTxRef::getBlockTimestamp() const
{
   StoredHeader sbh;

   if(dbKey6B_.getSize() == 6)
   {
      db_->getStoredHeader(sbh, getBlockHeight(), getDuplicateID(), false);
      return READ_UINT32_LE(sbh.dataCopy_.getPtr()+68);
   }
   else
      return UINT32_MAX;
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBTxRef::getBlockHash(void) const
{
   StoredHeader sbh;
   if(dbKey6B_.getSize() == 6)
   {
      db_->getStoredHeader(sbh, getBlockHeight(), getDuplicateID(), false);
      return sbh.thisHash_;
   }
   else
      return BtcUtils::EmptyHash();
}

////////////////////////////////////////////////////////////////////////////////
TxIn  DBTxRef::getTxInCopy(uint32_t i)  
{
   return db_->getTxInCopy( dbKey6B_, i);
}

////////////////////////////////////////////////////////////////////////////////
TxOut DBTxRef::getTxOutCopy(uint32_t i)
{
   return db_->getTxOutCopy(dbKey6B_, i);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// UnspentTxOut Methods
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
UnspentTxOut::UnspentTxOut(void) :
   txHash_(BtcUtils::EmptyHash()),
   txOutIndex_(0),
   txHeight_(0),
   value_(0),
   script_(BinaryData(0)),
   isMultisigRef_(false)
{
   // Nothing to do here
}

////////////////////////////////////////////////////////////////////////////////
BinaryData UnspentTxOut::getRecipientScrAddr(void) const
{
   return BtcUtils::getTxOutScrAddr(getScript());
}


////////////////////////////////////////////////////////////////////////////////
uint32_t UnspentTxOut::getNumConfirm(uint32_t currBlkNum) const
{
   if (txHeight_ == UINT32_MAX)
      throw runtime_error("uninitiliazed UnspentTxOut");
   
   return currBlkNum - txHeight_ + 1;
}

////////////////////////////////////////////////////////////////////////////////
bool UnspentTxOut::CompareNaive(UnspentTxOut const & uto1, 
                                UnspentTxOut const & uto2)
{
   float val1 = (float)uto1.getValue();
   float val2 = (float)uto2.getValue();
   return (val1*uto1.txHeight_ < val2*uto2.txHeight_);
}

////////////////////////////////////////////////////////////////////////////////
bool UnspentTxOut::CompareTech1(UnspentTxOut const & uto1,
                                UnspentTxOut const & uto2)
{
   float val1 = pow((float)uto1.getValue(), 1.0f/3.0f);
   float val2 = pow((float)uto2.getValue(), 1.0f/3.0f);
   return (val1*uto1.txHeight_ < val2*uto2.txHeight_);

}

////////////////////////////////////////////////////////////////////////////////
bool UnspentTxOut::CompareTech2(UnspentTxOut const & uto1,
                                UnspentTxOut const & uto2)
{
   float val1 = pow(log10((float)uto1.getValue()) + 5, 5);
   float val2 = pow(log10((float)uto2.getValue()) + 5, 5);
   return (val1*uto1.txHeight_ < val2*uto2.txHeight_);

}

////////////////////////////////////////////////////////////////////////////////
bool UnspentTxOut::CompareTech3(UnspentTxOut const & uto1,
                                UnspentTxOut const & uto2)
{
   float val1 = pow(log10((float)uto1.getValue()) + 5, 4);
   float val2 = pow(log10((float)uto2.getValue()) + 5, 4);
   return (val1*uto1.txHeight_ < val2*uto2.txHeight_);
}


////////////////////////////////////////////////////////////////////////////////
void UnspentTxOut::sortTxOutVect(vector<UnspentTxOut> & utovect, int sortType)
{
   switch(sortType)
   {
   case 0: sort(utovect.begin(), utovect.end(), CompareNaive); break;
   case 1: sort(utovect.begin(), utovect.end(), CompareTech1); break;
   case 2: sort(utovect.begin(), utovect.end(), CompareTech2); break;
   case 3: sort(utovect.begin(), utovect.end(), CompareTech3); break;
   default: break; // do nothing
   }
}


////////////////////////////////////////////////////////////////////////////////
void UnspentTxOut::pprintOneLine(uint32_t currBlk)
{
   printf(" Tx:%s:%02d   BTC:%0.3f   nConf:%04d\n",
             txHash_.copySwapEndian().getSliceCopy(0,8).toHexStr().c_str(),
             txOutIndex_,
             value_/1e8,
             getNumConfirm(currBlk));
}

// kate: indent-width 3; replace-tabs on;

