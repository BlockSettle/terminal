#include "ExplorerWidget.h"

#include <QDebug>
#include <QTimer>
#include "PlainWallet.h"
#include "ui_ExplorerWidget.h"
#include "BlocksViewModel.h"

namespace {

const unsigned kLatestBlockCount = 10;

const size_t kBlockHashSize = 32;

bool decodeHexHash(const QString& text, BinaryData *hash) {
   if (text.length() != kBlockHashSize * 2) {
      return false;
   }

   QByteArray h = QByteArray::fromHex(text.toLatin1());
   if (h.size() != kBlockHashSize) {
      return false;
   }

   hash->copyFrom(h.toStdString());
   return true;
}

}

ExplorerWidget::ExplorerWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ExplorerWidget)
{
   ui_->setupUi(this);

   connect(ui_->lineEditSearch, &QLineEdit::returnPressed, this, &ExplorerWidget::onSearchReturnPressed);

   ui_->stackedWidget->setCurrentIndex(int(Pages::Overview));
}

ExplorerWidget::~ExplorerWidget() = default;

void ExplorerWidget::init(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ArmoryConnection> &armory)
{
   logger_ = logger;
   armory_ = armory;

   blocksViewModel_ = new BlocksViewModel(logger, armory, this);
   ui_->tableViewLatestBlocks->setModel(blocksViewModel_);

   connect(armory.get(), &ArmoryConnection::stateChanged, this, &ExplorerWidget::onArmoryStateChanged);

   QTimer::singleShot(2000, this, [this, armory] () {
      qDebug() << "top block:" << armory_->topBlock();

      auto wallet = new bs::PlainWallet();

      connect(wallet, &bs::PlainWallet::walletReady, [this, wallet](const QString &walletId) {
         qDebug() << "wallet ready";
         QTimer::singleShot(10, this, [wallet]() {
            wallet->UpdateBalanceFromDB([wallet](std::vector<uint64_t> balance) {
               qDebug() << wallet->GetSpendableBalance() << balance;
               qDebug() << wallet->isBalanceAvailable();
               qDebug() << wallet->getAddrBalance(std::string("tb1q3cqhqq4vpsy20tvd9e8parg952xp5cct7tyfst"), [](std::vector<uint64_t> balance) {
                  qDebug() << "getAddrBalance after wallet ready" << balance;
               });
            });
         });
      });

      connect(armory.get(), &ArmoryConnection::refresh, [wallet](std::vector<BinaryData> data) {
         qDebug() << "armory refresh";
         wallet->UpdateBalanceFromDB([wallet](std::vector<uint64_t> balance) {
            qDebug() << wallet->GetSpendableBalance() << wallet->isBalanceAvailable() << balance;
         });
      });

      wallet->addAddress(std::string("tb1q3cqhqq4vpsy20tvd9e8parg952xp5cct7tyfst"));
      wallet->addAddress(std::string("tb1qnqnxn72t26ppc3ppdsf7847h4yggvvawm37skh"));

      auto result = wallet->RegisterWallet(armory);
   });

   QTimer::singleShot(2000, this, [this, armory] () {
      BinaryData txHash;
      bool result = decodeHexHash(QLatin1String("1a3a800708cb910948f574007451d5ab51de27dbed81054bff567fd7f727aec8"), &txHash);
      if (!result) {
         return;
      }

      txHash.swapEndian();

      armory_->getTxByHash(txHash, [](Tx tx) {
         auto txOutCount = tx.getNumTxOut();
         for (int i = 0; i < txOutCount; i++) {
            auto txOut = tx.getTxOutCopy(i);
            auto data = txOut.getScrAddressStr().getSliceCopy(1, 20);
            qDebug() << QString::fromLatin1(BtcUtils::scrAddrToSegWitAddress(data).toBinStr().c_str());
         }

         vector<size_t> offsetsWitness;
         BtcUtils::TxCalcLength(tx.getPtr(), tx.getSize(), nullptr, nullptr, &offsetsWitness);

         for (int i = 0; i < offsetsWitness.size() - 1; i++) {
            auto ptr = tx.getPtr() + offsetsWitness[i];
            int count = BtcUtils::readVarInt(ptr, 100);
            ptr += BtcUtils::readVarIntLength(ptr);
            qDebug() << count;
            for (int j = 0; j < count; ++j) {
               int size = BtcUtils::readVarInt(ptr, 100);
               ptr += BtcUtils::readVarIntLength(ptr);
               if (size == 33) {
                  BinaryData out;
                  BtcUtils::getHash160(ptr, size, out);
                  qDebug() << QString::fromLatin1(BtcUtils::scrAddrToSegWitAddress(out).toBinStr().c_str());
               }
               ptr += size;
               qDebug() << "size" << size;
            }
         }
      });
   });


   QTimer::singleShot(2000, this, [this, armory] () {
      BinaryData txHash;
      bool result = decodeHexHash(QLatin1String("a01d983d35616e2c60d6c3c82b481cab6fe15eb4dd0b018930b0f45b1d7ef680"), &txHash);
      if (!result) {
         return;
      }

      txHash.swapEndian();

      armory_->getTxByHash(txHash, [](Tx tx) {
         auto txInCount = tx.getNumTxIn();
         for (int i = 0; i < txInCount; i++) {
            auto txIn = tx.getTxInCopy(i);
            BinaryData addr;
            addr.append(0x6F).append(txIn.getSenderScrAddrIfAvail());
            auto data = BtcUtils::scrAddrToBase58(addr);
            qDebug() << QString::fromLatin1(data.toBinStr().c_str());
         }

         auto txOutCount = tx.getNumTxOut();
         for (int i = 0; i < txOutCount; i++) {
            auto txOut = tx.getTxOutCopy(i);
            auto addr = txOut.getScrAddressStr();
            auto data = BtcUtils::scrAddrToBase58(addr);
            qDebug() << QString::fromLatin1(data.toBinStr().c_str());
         }
      });
   });

   QTimer::singleShot(2000, this, [this, armory] () {
      BinaryData txHash;
      bool result = decodeHexHash(QLatin1String("08205db8eda491a53a0aa3a3079391d98602db0307c847b3e64c0623e0e48fc7"), &txHash);
      if (!result) {
         return;
      }

      txHash.swapEndian();

      armory_->getTxByHash(txHash, [](Tx tx) {
         tx.pprint(std::cout, 0, false);

         auto txInCount = tx.getNumTxIn();
         for (int i = 0; i < txInCount; i++) {
            auto txIn = tx.getTxInCopy(i);
            BinaryData addr;
            addr.append(0x6F).append(txIn.getSenderScrAddrIfAvail());
            auto data = BtcUtils::scrAddrToBase58(addr);
            qDebug() << QString::fromLatin1(data.toBinStr().c_str());
         }

         auto txOutCount = tx.getNumTxOut();
         for (int i = 0; i < txOutCount; i++) {
            auto txOut = tx.getTxOutCopy(i);
            auto script = txOut.getScript();

            auto ptr = script.getPtr();
            int version = BtcUtils::readVarInt(ptr, 100);
            qDebug() << version;
            ptr += BtcUtils::readVarIntLength(ptr);

            int size = BtcUtils::readVarInt(ptr, 100);
            qDebug() << size;
            ptr += BtcUtils::readVarIntLength(ptr);

            auto addr = script.getSliceRef(2, 32);
            qDebug() << QString::fromLatin1(BtcUtils::scrAddrToSegWitAddress(addr).toBinStr().c_str());
         }
      });
   });

   QTimer::singleShot(2000, this, [this, armory] () {
      BinaryData txHash;
      bool result = decodeHexHash(QLatin1String("dd3ac46414018eddbc0e39722154891f733e2e005533491edecdb8dd33a43b79"), &txHash);
      if (!result) {
         return;
      }

      txHash.swapEndian();

      armory_->getTxByHash(txHash, [](Tx tx) {
         tx.pprint(std::cout, 0, false);

         vector<size_t> offsetsWitness;
         BtcUtils::TxCalcLength(tx.getPtr(), tx.getSize(), nullptr, nullptr, &offsetsWitness);

         for (int i = 0; i < offsetsWitness.size() - 1; i++) {
            qDebug() << "process witness" << i;

            int size = offsetsWitness[i + 1] - offsetsWitness[i];
            auto ptr = tx.getPtr() + offsetsWitness[i];

            int itemCount = BtcUtils::readVarInt(ptr, size);
            ptr += BtcUtils::readVarIntLength(ptr);
            size -= BtcUtils::readVarIntLength(ptr);
            qDebug() << "itemCount" << itemCount;

            for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
               int itemSize = BtcUtils::readVarInt(ptr, size);
               ptr += BtcUtils::readVarIntLength(ptr);
               size -= BtcUtils::readVarIntLength(ptr);
               qDebug() << "itemSize" << itemSize;

               if (itemIndex == itemCount - 1) {
                  BinaryData hash;
                  BtcUtils::getSha256(ptr, itemSize, hash);
                  qDebug() << QString::fromLatin1(BtcUtils::scrAddrToSegWitAddress(hash).toBinStr().c_str());
               }

               ptr += itemSize;
            }

//            qDebug() << version;
//            for (int j = 0; j < count; ++j) {
//               int size = BtcUtils::readVarInt(ptr, 100);
//               ptr += BtcUtils::readVarIntLength(ptr);
//               if (size == 33) {
//                  BinaryData out;
//                  BtcUtils::getHash160(ptr, size, out);
//                  qDebug() << QString::fromLatin1(BtcUtils::scrAddrToSegWitAddress(out).toBinStr().c_str());
//               }
//               ptr += size;
//               qDebug() << "size" << size;
//            }
         }
      });
   });
}

void ExplorerWidget::onArmoryStateChanged(ArmoryConnection::State state)
{
   if (state == ArmoryConnection::State::Ready) {
      updateOverview();
   }
}

void ExplorerWidget::onSearchReturnPressed()
{
   search(ui_->lineEditSearch->text());
}

void ExplorerWidget::search(const QString &text)
{
   BinaryData blockHash;
   bool result = decodeHexHash(text, &blockHash);
   if (result) {
      armory_->getBlockHeaderByHash(blockHash, [this](ClientClasses::BlockHeader blockHeader) {
         showBlockHeader(blockHeader);
      });
   }
}

void ExplorerWidget::showBlockHeader(ClientClasses::BlockHeader &blockHeader)
{
   ui_->stackedWidget->setCurrentIndex(int(Pages::Block));
}

void ExplorerWidget::updateOverview()
{
   blocksViewModel_->updateFromTopBlocks();
}
