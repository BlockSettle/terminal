/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "QmlFactory.h"

#include <QApplication>
#include <QStyle>
#include <QClipboard>
#include <QQuickWindow>
#include <QKeyEvent>
#include <QDir>

#include <spdlog/spdlog.h>
#include "Wallets/SyncWalletsManager.h"
#include "SignerAdapter.h"

#include "Bip39EntryValidator.h"

using namespace bs::hd;

// todo
// check authObject->signWallet results, return null object, emit error signal

QmlFactory::QmlFactory(const std::shared_ptr<ApplicationSettings> &settings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<spdlog::logger> &logger
   , QObject *parent)
   : QObject(parent)
   , settings_(settings)
   , connectionManager_(connectionManager)
   , logger_(logger)
{
}

void QmlFactory::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr)
{
   walletsMgr_ = walletsMgr;
}

bs::wallet::QSeed* QmlFactory::createSeedFromMnemonic(const QString &key, bool isTestNet)
{
   auto networkType = isTestNet ? bs::wallet::QSeed::QNetworkType::TestNet : bs::wallet::QSeed::QNetworkType::MainNet;
   auto seed = new bs::wallet::QSeed(bs::wallet::QSeed::fromMnemonicWordList(key, networkType, bip39Dictionaries()));
   QQmlEngine::setObjectOwnership(seed, QQmlEngine::JavaScriptOwnership);
   return seed;
}

WalletInfo *QmlFactory::createWalletInfo() const{
   auto wi = new bs::hd::WalletInfo();
   QQmlEngine::setObjectOwnership(wi, QQmlEngine::JavaScriptOwnership);
   return wi;
}

WalletInfo *QmlFactory::createWalletInfo(const QString &walletId) const
{
   if (!walletsMgr_) {
      logger_->error("[{}] wallets manager is missing", __func__);
      return nullptr;
   }
   // ? move logic to WalletsManager ?
   bs::hd::WalletInfo *wi = nullptr;

   const auto &wallet = walletsMgr_->getWalletById(walletId.toStdString());
   if (wallet) {
      wi = new bs::hd::WalletInfo(walletsMgr_, wallet);
   }
   else {
      const auto &hdWallet = walletsMgr_->getHDWalletById(walletId.toStdString());
      if (!hdWallet) {
         logger_->warn("[{}] wallet with id {} not found", __func__, walletId.toStdString());
         wi = new bs::hd::WalletInfo();
      }
      else {
         wi = new bs::hd::WalletInfo(walletsMgr_, hdWallet);
      }
   }

   QQmlEngine::setObjectOwnership(wi, QQmlEngine::JavaScriptOwnership);
   return wi;
}

bs::hd::WalletInfo *QmlFactory::createWalletInfo(int index) const
{
   const auto &hdWallets = walletsMgr_->hdWallets();
   if ((index < 0) || (index >= hdWallets.size())) {
      return nullptr;
   }
   const auto &wallet = hdWallets[index];
   auto wi = new bs::hd::WalletInfo(walletsMgr_, wallet);
   QQmlEngine::setObjectOwnership(wi, QQmlEngine::JavaScriptOwnership);
   return wi;
}

bs::hd::WalletInfo* QmlFactory::createWalletInfoFromDigitalBackup(const QString& filename) const
{
   auto wi = new bs::hd::WalletInfo(bs::hd::WalletInfo::fromDigitalBackup(filename));
   QQmlEngine::setObjectOwnership(wi, QQmlEngine::JavaScriptOwnership);
   return wi;
}

void QmlFactory::setClipboard(const QString &text) const
{
   QApplication::clipboard()->setText(text);
}

QString QmlFactory::getClipboard() const
{
   return QApplication::clipboard()->text();
}

QRect QmlFactory::frameSize(QObject *window) const
{
   auto win = qobject_cast<QQuickWindow *>(window);
   if (win) {
      return win->frameGeometry();
   }
   return QRect();
}

int QmlFactory::titleBarHeight()
{
   return QApplication::style()->pixelMetric(QStyle::PM_TitleBarHeight);
}

void QmlFactory::installEventFilterToObj(QObject *object)
{
   if (!object) {
      return;
   }

   object->installEventFilter(this);
}

void QmlFactory::applyWindowFix(QQuickWindow *mw)
{
#ifdef Q_OS_WIN
   SetClassLongPtr(HWND(mw->winId()), GCLP_HBRBACKGROUND, LONG_PTR(GetStockObject(NULL_BRUSH)));
#endif
}

bool QmlFactory::eventFilter(QObject *object, QEvent *event)
{
   if (event->type() == QEvent::Close) {
      emit closeEventReceived();
      event->ignore();
      return true;
   }

   return false;
}

bool QmlFactory::isDebugBuild()
{
#ifndef NDEBUG
   return true;
#else
   return false;
#endif
}

QString QmlFactory::headlessPubKey() const
{
   return headlessPubKey_;
}

void QmlFactory::setHeadlessPubKey(const QString &headlessPubKey)
{
   if (headlessPubKey != headlessPubKey_) {
      headlessPubKey_ = headlessPubKey;
      emit headlessPubKeyChanged();
   }
}

int QmlFactory::controlPasswordStatus() const
{
   // This method is using in QML so that's why we return int type 
   return static_cast<int>(controlPasswordStatus_);
}

void QmlFactory::setControlPasswordStatus(int controlPasswordStatus)
{
    controlPasswordStatus_ = static_cast<ControlPasswordStatus::Status>(controlPasswordStatus);
}

bool QmlFactory::initMessageWasShown() const
{
    return isControlPassMessageShown;
}

void QmlFactory::setInitMessageWasShown()
{
    isControlPassMessageShown = true;
}

const std::vector<std::vector<std::string>>& QmlFactory::bip39Dictionaries()
{
   if (!bip39Dictionaries_.empty()) {
      return bip39Dictionaries_;
   }

   // Lazy init from resources
   QDir dictLocation(QLatin1String(":/bip39Dictionaries/"));
   QStringList dictFiles = dictLocation.entryList(QStringList(QLatin1String("*.txt")));
   bip39Dictionaries_.reserve(dictFiles.size());
   for (const auto &dictFileName : dictFiles) {
      QFile dictFile(QString::fromLatin1("://bip39Dictionaries/%1").arg(dictFileName));
      if (!dictFile.open(QFile::ReadOnly)) {
         continue;
      }

      QTextStream fileStream(&dictFile);
      std::vector<std::string> dictionary;
      while (!fileStream.atEnd()) {
         dictionary.push_back(fileStream.readLine().toStdString());
      }
      // According to standard dictionary size 2048 words
      assert(dictionary.size() == 2048);
      bip39Dictionaries_.push_back(std::move(dictionary));
      dictFile.close();

      // We wanted English dictionary to be always in first place
      // to avoid lookup in other, with less chance to be use
      if (dictFileName == QLatin1String("english.txt")) {
         std::iter_swap(bip39Dictionaries_.begin(), bip39Dictionaries_.end() - 1);
      }
   }
   assert(!bip39Dictionaries_.empty());

   return bip39Dictionaries_;
}
