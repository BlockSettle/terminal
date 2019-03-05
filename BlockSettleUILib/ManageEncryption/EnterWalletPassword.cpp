#include <QSpacerItem>
#include "EnterWalletPassword.h"
#include "WalletKeysSubmitWidget.h"
#include <spdlog/spdlog.h>

using namespace bs::wallet;
using namespace bs::hd;

EnterWalletPassword::EnterWalletPassword(AutheIDClient::RequestType requestType
                                               , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::EnterWalletPassword())
   , requestType_(requestType)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &EnterWalletPassword::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &EnterWalletPassword::reject);

   connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, [this] { updateState(); });
}

EnterWalletPassword::~EnterWalletPassword() = default;

void EnterWalletPassword::init(const WalletInfo &walletInfo
                                  , const std::shared_ptr<ApplicationSettings> &appSettings
                                  , const std::shared_ptr<ConnectionManager> &connectionManager
                                  , WalletKeyWidget::UseType useType
                                  , const QString &prompt
                                  , const std::shared_ptr<spdlog::logger> &logger
                                  , const QString &title)
{
   assert (useType == WalletKeyWidget::UseType::RequestAuthAsDialog
           || useType == WalletKeyWidget::UseType::ChangeToPasswordAsDialog
           || useType == WalletKeyWidget::UseType::ChangeToEidAsDialog);

   assert (!walletInfo.encTypes().isEmpty());

   if (useType == WalletKeyWidget::UseType::ChangeToEidAsDialog)
      assert (!walletInfo.encKeys().isEmpty());

   walletInfo_ = walletInfo;
   appSettings_ = appSettings;
   logger_ = logger;

   ui_->labelAction->setText(prompt);
   ui_->labelWalletId->setText(tr("Wallet ID: %1").arg(walletInfo.rootId()));
   ui_->labelWalletName->setText(tr("Wallet name: %1").arg(walletInfo.name()));

   if (!title.isEmpty()) {
      setWindowTitle(title);
   }

   if (walletInfo_.isEidAuthOnly() || useType == WalletKeyWidget::UseType::ChangeToEidAsDialog) {
      connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, this, &EnterWalletPassword::accept);
      connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::failed, this, &EnterWalletPassword::reject);

      ui_->pushButtonOk->hide();
      ui_->spacerLeft->changeSize(1, 1, QSizePolicy::Expanding, QSizePolicy::Preferred);
      ui_->groupBoxSubmitKeys->setTitle(QStringLiteral());
   }

   ui_->widgetSubmitKeys->init(requestType_, walletInfo_, useType, logger_, appSettings_, connectionManager, prompt);
   ui_->widgetSubmitKeys->setFocus();
   ui_->widgetSubmitKeys->resume();

   updateState();

   adjustSize();
   setMinimumSize(size());


   if (useType == WalletKeyWidget::UseType::ChangeToEidAsDialog) {
      connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, this, &EnterWalletPassword::accept);
      connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::failed, this, &EnterWalletPassword::reject);

      ui_->pushButtonOk->hide();
      ui_->spacerLeft->changeSize(1,1, QSizePolicy::Expanding, QSizePolicy::Preferred);
   }
}

void EnterWalletPassword::updateState()
{
   ui_->pushButtonOk->setEnabled(ui_->widgetSubmitKeys->isValid());
}

void EnterWalletPassword::reject()
{
   ui_->widgetSubmitKeys->cancel();
   QDialog::reject();
}

SecureBinaryData EnterWalletPassword::resultingKey() const
{
    return ui_->widgetSubmitKeys->key();
}
