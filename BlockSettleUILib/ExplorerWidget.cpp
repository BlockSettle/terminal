#include "ExplorerWidget.h"
#include "ui_ExplorerWidget.h"
#include "UiUtils.h"
#include "TransactionDetailsWidget.h"
#include "BSMessageBox.h"

#include <QStringListModel>
#include <QToolTip>

// Overloaded constuctor. Does basic setup and Qt signal connection.
ExplorerWidget::ExplorerWidget(QWidget *parent) :
   TabWithShortcut(parent)
   , expTimer_(new QTimer)
   , ui_(new Ui::ExplorerWidget())
   , transactionHistoryPosition_(-1)
{
   ui_->setupUi(this);
   ui_->searchBox->setReadOnly(true);

   // Set up the explorer expiration timer.
   expTimer_->setInterval(EXP_TIMEOUT);
   expTimer_->setSingleShot(true);
   expTimer_->callOnTimeout(this, &ExplorerWidget::onExpTimeout);

   // connection to handle enter key being pressed inside the search box
   connect(ui_->searchBox, &QLineEdit::returnPressed,
           this, &ExplorerWidget::onSearchStarted);
   // connection to handle user clicking on TXID inside address details page
   connect(ui_->Address, &AddressDetailsWidget::transactionClicked,
           this, &ExplorerWidget::onTransactionClicked);
   // connection to handle user clicking on adress id inside tx details page
   connect(ui_->Transaction, &TransactionDetailsWidget::addressClicked,
           this, &ExplorerWidget::onAddressClicked);
   connect(ui_->btnSearch, &QPushButton::clicked,
           this, &ExplorerWidget::onSearchStarted);
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
   , const std::shared_ptr<spdlog::logger> &inLogger)
{
   armory_ = armory;
   logger_ = inLogger;
   ui_->Transaction->init(armory, inLogger, expTimer_);
   ui_->Address->init(armory, inLogger, expTimer_);
//   ui_->Block->init(armory, inLogger);

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

// The function called when the user uses the search bar (Tx or address).
void ExplorerWidget::onSearchStarted()
{
   const QString& userStr = ui_->searchBox->text();
   if (userStr.isEmpty()) {
      QToolTip::showText(ui_->searchBox->mapToGlobal(QPoint(0, 7)),
                         tr("Provide a valid address or transaction id."),
                         ui_->searchBox);
      return;
   }

   // Check if this is an address first. Check Base58 and Bech32. 32 byte hex will
   // just cause the system to think it's a P2SH (?) address.
   bool strIsAddress = false;
   bs::Address bsAddress;
   try {
      bsAddress = bs::Address(userStr.trimmed(), bs::Address::Format::Base58);
      strIsAddress = bsAddress.isValid();
   } catch (...) {}
   if(strIsAddress == false) {
      try {
         bsAddress = bs::Address(userStr.trimmed(), bs::Address::Format::Bech32);
         strIsAddress = bsAddress.isValid();
      } catch (...) {}
   }

   // If address, process. If not, see if it's a 32 byte (64 char) hex string.
   // Idx 0 = Block (BlockDetailsWidget - Not used for now)
   // Idx 1 = Tx (TransactionDetailsWidget)
   // Idx 2 = Address (AddressDetailsWidget)
   if(strIsAddress == true) {
      ui_->stackedWidget->setCurrentIndex(AddressPage);

      // Pass the address to the address widget and load the wallet, which kicks
      // off address processing and UI loading.
      ui_->Address->setQueryAddr(bsAddress);
      ui_->searchBox->clear();
      expTimer_->start();
   }
   else if(userStr.length() == 64 &&
           userStr.toStdString().find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos) {
      // String is a valid 32 byte hex string, so we may proceed.
      clearTransactionHistory();
      pushTransactionHistory(userStr);
      setTransaction(userStr);
      ui_->searchBox->clear();
      expTimer_->start();
   }
   else {
      // This isn't a valid address or 32 byte hex string.
      QToolTip::showText(ui_->searchBox->mapToGlobal(QPoint(0, 7)),
                         tr("This is not a valid address or transaction ID."),
                         ui_->searchBox);
   }
}

// This slot function is called whenever user clicks on a transaction in
// address details page or any other page.
void ExplorerWidget::onTransactionClicked(QString txId)
{
   truncateTransactionHistory();
   pushTransactionHistory(txId);
   setTransaction(txId);
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
   bool strIsBase58 = false;
   try {
      bsAddress = bs::Address(addressId.trimmed(), bs::Address::Format::Base58);
      strIsBase58 = bsAddress.isValid();
   } catch (...) {}
   if(strIsBase58 == false) {
      try {
         bsAddress = bs::Address(addressId.trimmed(), bs::Address::Format::Bech32);
         strIsBase58 = bsAddress.isValid();
      } catch (...) {}
   }

   // There really should be an error case here, but for now, assume addr is
   // valid. (It would be very bad if Armory fed up bad addresses!)
   // TO DO: Add a check for wallets that have already been loaded?
   ui_->Address->setQueryAddr(bsAddress);

   expTimer_->start();
}

void ExplorerWidget::onReset()
{
   expTimer_->stop();
   ui_->stackedWidget->setCurrentIndex(BlockPage);
   ui_->searchBox->clear();
   clearTransactionHistory();
}

void ExplorerWidget::onBackButtonClicked()
{
   if (transactionHistoryPosition_ > 0) {
      --transactionHistoryPosition_;
      const auto txId = transactionHistory_.at(static_cast<size_t>(transactionHistoryPosition_));
      setTransaction(QString::fromStdString(txId));
   }
}

void ExplorerWidget::onForwardButtonClicked()
{
   if (transactionHistoryPosition_ < static_cast<int>(transactionHistory_.size()) - 1) {
      ++transactionHistoryPosition_;
      const auto txId = transactionHistory_.at(static_cast<size_t>(transactionHistoryPosition_));
      setTransaction(QString::fromStdString(txId));
   }
}

bool ExplorerWidget::canGoBack() const
{
   return transactionHistoryPosition_ > 0;
}

bool ExplorerWidget::canGoForward() const
{
   return transactionHistoryPosition_ < static_cast<int>(transactionHistory_.size()) - 1;
}

void ExplorerWidget::setTransaction(QString txId)
{
   ui_->btnBack->setEnabled(canGoBack());
   ui_->btnForward->setEnabled(canGoForward());

   ui_->stackedWidget->setCurrentIndex(TxPage);
   // Pass the Tx hash to the Tx widget and populate the fields.
   BinaryTXID terminalTXID(READHEX(txId.toStdString()), true);
   ui_->Transaction->populateTransactionWidget(terminalTXID);
}

void ExplorerWidget::pushTransactionHistory(QString txId)
{
   if (txId.isEmpty())
      return;
   auto lastId = transactionHistory_.empty() ? std::string() : transactionHistory_.back();
   if (txId.toStdString() == lastId)
      return;
   transactionHistory_.push_back(txId.toStdString());
   transactionHistoryPosition_ = static_cast<int>(transactionHistory_.size()) - 1;
}

void ExplorerWidget::truncateTransactionHistory(int position)
{
   int pos = position >= 0 ? position : transactionHistoryPosition_;
   while (static_cast<int>(transactionHistory_.size()) - 1 > pos) {
      transactionHistory_.pop_back();
   }
}

void ExplorerWidget::clearTransactionHistory()
{
   transactionHistoryPosition_ = -1;
   truncateTransactionHistory();
   ui_->btnBack->setEnabled(canGoBack());
   ui_->btnForward->setEnabled(canGoForward());
}
