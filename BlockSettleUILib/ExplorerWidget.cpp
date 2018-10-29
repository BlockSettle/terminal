#include "ExplorerWidget.h"
#include <QStringListModel>
#include "ui_ExplorerWidget.h"
#include "UiUtils.h"
#include "BinaryData.h"

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
/*   ui_->stackedWidget->count();
   int index = ui_->stackedWidget->currentIndex();
   if (index < ui_->stackedWidget->count() - 1)	{
      ui_->stackedWidget->setCurrentIndex(++index);
   }
   else {
      ui_->stackedWidget->setCurrentIndex(0);
   }*/
   const QString& userStr = ui_->searchBox->text();
   if (userStr.isEmpty()) {
      // TO DO: TOSS UP AN ERROR WINDOW HERE.
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
      ui_->Address->setAddrVal(address_);

      // TO DO: Pass the address to the address widget and populate the fields.
   }
   else if(userStr.length() == 64 &&
           userStr.toStdString().find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos) {
      // String is a valid 32 byte hex string, so we may proceed.
      ui_->stackedWidget->setCurrentIndex(1);
      const BinaryData inHex = READHEX(userStr.toStdString());

      // TO DO: Pass the Tx hash to the Tx widget and populate the fields.
   }
   else {
      // TO DO: Toss up a window letting the user know this isn't a valid
      // address or Tx hash.
   }
}
