#include "ExplorerWidget.h"
#include "ui_ExplorerWidget.h"
#include "UiUtils.h"
#include "BinaryData.h"

#include <QStringListModel>
#include <QToolTip>

// Overloaded constuctor. Does basic setup and Qt signal connection.
ExplorerWidget::ExplorerWidget(QWidget *parent) :
   TabWithShortcut(parent)
   , ui_(new Ui::ExplorerWidget())
{
   ui_->setupUi(this);

   connect(ui_->searchBox, &QLineEdit::returnPressed,
           this, &ExplorerWidget::onSearchStarted);
   // connection to handle enter key being pressed inside the search box
    connect(ui_->searchBox, &QLineEdit::returnPressed,
            this, &ExplorerWidget::onSearchStarted);
   // connection to handle user clicking on transaction id inside address details page
   connect(ui_->Address, &AddressDetailsWidget::transactionClicked,
           this, &ExplorerWidget::onTransactionClicked);
   // connection to handle user clicking on adress id inside tx details page
   connect(ui_->Transaction, &TransactionDetailsWidget::addressClicked,
           this, &ExplorerWidget::onAddressClicked);
}

ExplorerWidget::~ExplorerWidget() = default;

// Initialize the widget and related widgets (block, address, Tx). Blocks won't
// be set up for now.
void ExplorerWidget::init(const std::shared_ptr<ArmoryConnection> &armory,
                          const std::shared_ptr<spdlog::logger> &inLogger)
{
   armory_ = armory;
   logger_ = inLogger;

   ui_->Transaction->init(armory, inLogger);
   ui_->Address->init(armory, inLogger);
//   ui_->Block->init(armory, inLogger);
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
   bool strIsAddress_ = false;
   bs::Address address_;
   try {
      address_ = bs::Address(userStr.trimmed(), bs::Address::Format::Base58);
      strIsAddress_ = address_.isValid();
   } catch (...) {}
   if(strIsAddress_ == false) {
      try {
         address_ = bs::Address(userStr.trimmed(), bs::Address::Format::Bech32);
         strIsAddress_ = address_.isValid();
      } catch (...) {}
   }

   // If address, process. If not, see if it's a 32 byte (64 char) hex string.
   // Idx 0 = Block (BlockDetailsWidget - Not used for now)
   // Idx 1 = Tx (TransactionDetailsWidget)
   // Idx 2 = Address (AddressDetailsWidget)
   if(strIsAddress_ == true) {
      ui_->stackedWidget->setCurrentIndex(AddressPage);

      // TO DO: Pass the address to the address widget and populate the fields.
      // this sets the address field
      ui_->Address->setAddrVal(address_);
      ui_->Address->loadWallet();
   }
   else if(userStr.length() == 64 &&
           userStr.toStdString().find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos) {
      // String is a valid 32 byte hex string, so we may proceed.
      ui_->stackedWidget->setCurrentIndex(TxPage);

      // TO DO: Pass the Tx hash to the Tx widget and populate the fields. Should do reversal there.
      ui_->Transaction->populateTransactionWidget(READHEX(userStr.toStdString()));
   }
   else {
      // This isn't a valid address or 32 byte hex string.
      QToolTip::showText(ui_->searchBox->mapToGlobal(QPoint(0, 7)),
                         tr("This is not a valid address or transaction id."),
                         ui_->searchBox);
   }
}

// This slot function is called whenever user clicks on a transaction in
// address details page or any other page.
void ExplorerWidget::onTransactionClicked(QString txId) {
//   ui_->Transaction->setTxVal(txId);
//   ui_->Transaction->getTxsForTxIns(); // Maybe moved?
//   ui_->Transaction->loadInputs();
   ui_->stackedWidget->setCurrentIndex(TxPage);
}

// This slot function is called whenever user clicks on an address in
// transaction details page or any other page.
void ExplorerWidget::onAddressClicked(QString addressId) {
   ui_->Address->setAddrVal(addressId);
   ui_->stackedWidget->setCurrentIndex(AddressPage);
}
