#include "ExplorerWidget.h"
#include <QStringListModel>
#include "ui_ExplorerWidget.h"
#include "UiUtils.h"
#include "BinaryData.h"
#include <QToolTip>

// Overloaded constuctor. Does basic setup and Qt signal connection.
ExplorerWidget::ExplorerWidget(QWidget *parent) :
   TabWithShortcut(parent)
   , ui_(new Ui::ExplorerWidget())
{
   ui_->setupUi(this);

   connect(ui_->searchBox, &QLineEdit::returnPressed,
           this, &ExplorerWidget::onSearchStarted);
}

ExplorerWidget::~ExplorerWidget() = default;

// Initialize the widget.
void ExplorerWidget::init(const std::shared_ptr<ArmoryConnection> &armory)
{
   armory_ = armory;
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
   // PSEUDOCODE: This needs to be coded properly.
   // - Get the text - ui_->stackedWidget->text()
   // - Convert to BinaryData and feed to a function that confirms if it's an
   //   address. CreateTransactionDialogAdvanced.cpp should have an example.
   // -- If an address, set index to 2 (Address) and pass along the address
   //    object so that it can populate the window.
   // - If not an address, check to see if it's a 32 byte hex string.
   // -- If so, attempt to get the Tx fom ArmoryConnection.
   // --- If success, set index to 1 (Tx) and pass along the Tx to populate
   //     the window.
   // --- If failure, issue a pop-up stating that the value is an invalid Tx
   //     reference.
   // - If not a valid byte string, issue a pop-up stating that the value is
   //   neither a valid address or Tx reference.
   // - BLOCKS ARE BEING IGNORED FOR NOW. ARMORY IS NOT DESIGNED TO SUPPORT
   //   BLOCK-SPECIFIC INFO AND WILL BLOW UP THE DB SIZE IF REDESIGNED TO ADD
   //   SUPPORT. It may be possible to query Core directly and parse the JSON
   //   output.

   const QString& userStr = ui_->searchBox->text();
   if (userStr.isEmpty()) {
      QToolTip::showText(ui_->searchBox->mapToGlobal(QPoint(0, 7)), tr("Provide a valid address or transaction id."), ui_->searchBox);
      return;
   }

   // Check if this is an address first.
   bool strIsAddress_ = false;
   bs::Address address_;
   try {
      address_ = bs::Address(userStr.trimmed());
      strIsAddress_ = address_.isValid();
   } catch (...) {}

   // If address, process. If not, see if it's a 32 byte (64 char) hex string.
   // Idx 0 = Block (BlockDetailsWidget - Not used for now)
   // Idx 1 = Tx (TransactionDetailsWidget)
   // Idx 2 = Address (AddressDetailsWidget)
   if(strIsAddress_ == true) {
      ui_->stackedWidget->setCurrentIndex(2);

      // TO DO: Pass the address to the address widget and populate the fields.
      // this sets the address field
      ui_->Address->setAddrVal(address_);
      // this populates the transactions tree
      ui_->Address->loadTransactions();
   }
   else if(userStr.length() == 64 &&
           userStr.toStdString().find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos) {
      // String is a valid 32 byte hex string, so we may proceed.
      ui_->stackedWidget->setCurrentIndex(1);
      const BinaryData inHex = READHEX(userStr.toStdString());
      
      // TO DO: Pass the Tx hash to the Tx widget and populate the fields.
      populateTransactionWidget(inHex);
   }
   else {
      // TO DO: Toss up a window letting the user know this isn't a valid
      // address or Tx hash.
      QToolTip::showText(ui_->searchBox->mapToGlobal(QPoint(0, 7)), tr("This is not a valid address or transaction id."), ui_->searchBox);
      
      //qDebug() << QLatin1String("[CreateTransactionDialogAdvanced::onCreatePressed] exception: ") << QString::fromStdString(e.what());
   }
}

// This function tries to use getTxByHash() to retrieve info about transaction. 
// Code was commented out because I could not get it to work and didn't want to spend too
// much time trying. At the bottom of the function are 2 calls, one that populates 
// transaction id in the page, and the other populates Inputs tree. 
void ExplorerWidget::populateTransactionWidget(const BinaryData inHex)
{
   // get the transaction data from armory
   /*
   const auto &cbTX = [this, entry, cb](Tx tx) {
      if (!tx.isInitialized()) {
         logger_->error("[bs::SettlementMonitor::IsPayInTransaction] TX not initialized for {}."
            , entry.getTxHash().toHexStr());
         cb(false);
         return;
      }

      for (int i = 0; i < tx.getNumTxOut(); ++i) {
         TxOut out = tx.getTxOutCopy(i);
         auto address = out.getScrAddressStr();
         if (ownAddresses_.find(address) != ownAddresses_.end()) {
            cb(true);
            return;
         }
      }
      cb(false);Z
   };*/
   //armory_->getTxByHash(inHex, cbTransactionReceived);

   ui_->Transaction->setTxRefVal(inHex);

   // Load the inputs table with dummy data, feel free to modify to work with
   // real data, or if you can get the getTxByHash() and the callback to work correctly
   // for me I can update it to populate it with actual data. 
   ui_->Transaction->loadInputs();
}