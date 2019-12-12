/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ExplorerWidget.h"
#include "ui_ExplorerWidget.h"
#include "AuthAddressManager.h"
#include "BSMessageBox.h"
#include "TransactionDetailsWidget.h"
#include "UiUtils.h"

#include <QMouseEvent>
#include <QStringListModel>
#include <QTimer>
#include <QToolTip>

namespace {

   constexpr auto ExplorerTimeout = std::chrono::seconds(10);

} // namespace

// Overloaded constuctor. Does basic setup and Qt signal connection.
ExplorerWidget::ExplorerWidget(QWidget *parent) :
   TabWithShortcut(parent)
   , ui_(new Ui::ExplorerWidget())
   , expTimer_(new QTimer)
   , searchHistoryPosition_(-1)
{
   ui_->setupUi(this);
   ui_->searchBox->setReadOnly(true);

   // Set up the explorer expiration timer.
   expTimer_->setInterval(ExplorerTimeout);
   expTimer_->setSingleShot(true);
   connect(expTimer_.get(), &QTimer::timeout, this, &ExplorerWidget::onExpTimeout);
   connect(ui_->Transaction, &TransactionDetailsWidget::finished, expTimer_.get(), &QTimer::stop);
   connect(ui_->Address, &AddressDetailsWidget::finished, expTimer_.get(), &QTimer::stop);

   // connection to handle enter key being pressed inside the search box
   connect(ui_->searchBox, &QLineEdit::returnPressed,
           this, [this](){ onSearchStarted(true); });
   // connection to handle user clicking on TXID inside address details page
   connect(ui_->Address, &AddressDetailsWidget::transactionClicked,
           this, &ExplorerWidget::onTransactionClicked);
   // connection to handle user clicking on adress id inside tx details page
   connect(ui_->Transaction, &TransactionDetailsWidget::addressClicked
      , this, &ExplorerWidget::onAddressClicked);
   connect(ui_->Transaction, &TransactionDetailsWidget::txHashClicked
      , this, &ExplorerWidget::onTransactionClicked);
   connect(ui_->btnSearch, &QPushButton::clicked,
           this, [this](){ onSearchStarted(true); });
   connect(ui_->btnReset, &QPushButton::clicked,
           this, &ExplorerWidget::onReset);
   connect(ui_->btnBack, &QPushButton::clicked,
           this, &ExplorerWidget::onBackButtonClicked);
   connect(ui_->btnForward, &QPushButton::clicked,
           this, &ExplorerWidget::onForwardButtonClicked);
}

ExplorerWidget::~ExplorerWidget() = default;

// Initialize the widget and related widgets (block, address, Tx). Blocks won't
// be set up for now.
void ExplorerWidget::init(const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<spdlog::logger> &inLogger
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<CCFileManager> &ccFileMgr
   , const std::shared_ptr<AuthAddressManager> &authMgr)
{
   logger_ = inLogger;
   authMgr_ = authMgr;
   ui_->Transaction->init(armory, inLogger, walletsMgr, ccFileMgr->getResolver());
   ui_->Address->init(armory, inLogger, ccFileMgr->getResolver());
//   ui_->Block->init(armory, inLogger);

   connect(authMgr_.get(), &AuthAddressManager::ConnectionComplete, [this] {
      ui_->Address->setBSAuthAddrs(authMgr_->GetBSAddresses());
   });

   // With Armory and the logger set, we can start accepting text input.
   ui_->searchBox->setReadOnly(false);
   ui_->searchBox->setPlaceholderText(QString::fromStdString(
      "Search for a transaction or address."));
}

void ExplorerWidget::shortcutActivated(ShortcutType s)
{
   switch (s) {

   default:
      break;
   }
}

void ExplorerWidget::mousePressEvent(QMouseEvent *event)
{
   if (event->button() == Qt::BackButton && ui_->btnBack->isEnabled()) {
      onBackButtonClicked();
   } else if (event->button() == Qt::ForwardButton && ui_->btnForward->isEnabled()) {
      onForwardButtonClicked();
   }
}

// The function called when the user uses the search bar (Tx or address).
void ExplorerWidget::onSearchStarted(bool saveToHistory)
{
   const QString& userStr = ui_->searchBox->text();
   if (userStr.isEmpty()) {
      QToolTip::showText(ui_->searchBox->mapToGlobal(QPoint(0, 7))
         , tr("Provide a valid address or transaction id."), ui_->searchBox);
      return;
   }

   // Check if this is an address first. Check Base58 and Bech32. 32 byte hex will
   // just cause the system to think it's a P2SH (?) address.
   bool strIsAddress = false;
   bs::Address bsAddress;
   try {
      bsAddress = bs::Address::fromAddressString(userStr.trimmed().toStdString());
      strIsAddress = bsAddress.isValid();
   } catch (const std::exception &) { }

   // If address, process. If not, see if it's a 32 byte (64 char) hex string.
   // Idx 0 = Block (BlockDetailsWidget - Not used for now)
   // Idx 1 = Tx (TransactionDetailsWidget)
   // Idx 2 = Address (AddressDetailsWidget)
   if (strIsAddress) {
      ui_->stackedWidget->setCurrentIndex(AddressPage);

      expTimer_->start();

      // Pass the address to the address widget and load the wallet, which kicks
      // off address processing and UI loading.
      ui_->Address->setQueryAddr(bsAddress);
      ui_->searchBox->clear();
      if (saveToHistory) {
         pushTransactionHistory(userStr);
      }
   }
   else if ((userStr.length() == 64) &&
           userStr.toStdString().find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos) {
      // String is a valid 32 byte hex string, so we may proceed.
      if (saveToHistory) {
         pushTransactionHistory(userStr);
      }
      expTimer_->start();
      setTransaction(userStr);
      ui_->searchBox->clear();
   }
   else {
      // This isn't a valid address or 32 byte hex string.
      QToolTip::showText(ui_->searchBox->mapToGlobal(QPoint(0, 7))
         , tr("This is not a valid address or transaction ID."), ui_->searchBox);
   }

   ui_->btnBack->setEnabled(canGoBack());
   ui_->btnForward->setEnabled(canGoForward());
}

// This slot function is called whenever user clicks on a transaction in
// address details page or any other page.
void ExplorerWidget::onTransactionClicked(QString txId)
{
   truncateSearchHistory();
   pushTransactionHistory(txId);
   setTransaction(txId);

   ui_->btnBack->setEnabled(canGoBack());
   ui_->btnForward->setEnabled(canGoForward());
}

// Function called when the explorer timeout expires. It just lets the user know
// that the explorer query took too long.
void ExplorerWidget::onExpTimeout()
{
   MessageBoxExpTimeout(this).exec();
}

// This slot function is called whenever user clicks on an address in
// transaction details page or any other page.
void ExplorerWidget::onAddressClicked(QString addressId)
{
   ui_->stackedWidget->setCurrentIndex(AddressPage);

   bs::Address bsAddress;
   try {
      bsAddress = bs::Address::fromAddressString(addressId.trimmed().toStdString());
   } catch (...) {}
   
   // There really should be an error case here, but for now, assume addr is
   // valid. (It would be very bad if Armory fed up bad addresses!)
   // TODO: Add a check for wallets that have already been loaded?
   ui_->Address->setQueryAddr(bsAddress);
   truncateSearchHistory();
   pushTransactionHistory(addressId);

   ui_->btnBack->setEnabled(canGoBack());
   ui_->btnForward->setEnabled(canGoForward());

   expTimer_->start();
}

void ExplorerWidget::onReset()
{
   expTimer_->stop();
   ui_->stackedWidget->setCurrentIndex(BlockPage);
   ui_->searchBox->clear();
}

void ExplorerWidget::onBackButtonClicked()
{
   if (searchHistoryPosition_ > 0) {
      --searchHistoryPosition_;
      const auto itemId = searchHistory_.at(static_cast<size_t>(searchHistoryPosition_));
      ui_->searchBox->setText(QString::fromStdString(itemId));
      onSearchStarted(false);
   }
}

void ExplorerWidget::onForwardButtonClicked()
{
   if (searchHistoryPosition_ < static_cast<int>(searchHistory_.size()) - 1) {
      ++searchHistoryPosition_;
      const auto itemId = searchHistory_.at(static_cast<size_t>(searchHistoryPosition_));
      ui_->searchBox->setText(QString::fromStdString(itemId));
      onSearchStarted(false);   }
}

bool ExplorerWidget::canGoBack() const
{
   return searchHistoryPosition_ > 0;
}

bool ExplorerWidget::canGoForward() const
{
   return searchHistoryPosition_ < static_cast<int>(searchHistory_.size()) - 1;
}

void ExplorerWidget::setTransaction(QString txId)
{
   ui_->stackedWidget->setCurrentIndex(TxPage);
   // Pass the Tx hash to the Tx widget and populate the fields.
   const TxHash terminalTXID(txId);
   ui_->Transaction->populateTransactionWidget(terminalTXID);
}

void ExplorerWidget::pushTransactionHistory(QString itemId)
{
   if (itemId.isEmpty())
      return;
   auto lastId = searchHistory_.empty() ? std::string() : searchHistory_.back();
   if (itemId.toStdString() == lastId)
      return;
   searchHistory_.push_back(itemId.toStdString());
   searchHistoryPosition_ = static_cast<int>(searchHistory_.size()) - 1;
}

void ExplorerWidget::truncateSearchHistory(int position)
{
   int pos = position >= 0 ? position : searchHistoryPosition_;
   while (static_cast<int>(searchHistory_.size()) - 1 > pos) {
      searchHistory_.pop_back();
   }
}

void ExplorerWidget::clearSearchHistory()
{
   searchHistoryPosition_ = -1;
   truncateSearchHistory();
   ui_->btnBack->setEnabled(canGoBack());
   ui_->btnForward->setEnabled(canGoForward());
}
