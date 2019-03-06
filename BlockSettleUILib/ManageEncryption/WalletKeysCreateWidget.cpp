#include "WalletKeysCreateWidget.h"
#include "ui_WalletKeysCreateWidget.h"
#include <set>
#include <QSpinBox>
#include "ApplicationSettings.h"


WalletKeysCreateWidget::WalletKeysCreateWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::WalletKeysCreateWidget())
{
   ui_->setupUi(this);
   ui_->pushButtonDelKey->setEnabled(false);
   ui_->labelRankN->clear();

   // currently we dont using m of n
   ui_->groupBox->hide();
   layout()->removeWidget(ui_->groupBox);

   connect(ui_->pushButtonAddKey, &QPushButton::clicked, this, &WalletKeysCreateWidget::onAddClicked);
   connect(ui_->pushButtonDelKey, &QPushButton::clicked, this, &WalletKeysCreateWidget::onDelClicked);
   connect(ui_->spinBoxRankM, SIGNAL(valueChanged(int)), this, SLOT(updateKeyRank(int)));
}

WalletKeysCreateWidget::~WalletKeysCreateWidget() = default;

void WalletKeysCreateWidget::setFlags(Flags flags)
{
   flags_ = flags;
}

void WalletKeysCreateWidget::init(AutheIDClient::RequestType requestType
                                     , const bs::hd::WalletInfo &walletInfo
                                     , WalletKeyWidget::UseType useType
                                     , const std::shared_ptr<ApplicationSettings>& appSettings
                                     , const std::shared_ptr<ConnectionManager> &connectionManager
                                     , const std::shared_ptr<spdlog::logger> &logger)
{
   requestType_ = requestType;
   walletInfo_ = walletInfo;
   appSettings_ = appSettings;
   connectionManager_ = connectionManager;
   logger_ = logger;
   useType_ = useType;

   widgets_.clear();
   pwdData_.clear();

   if (flags_ & HideGroupboxCaption) {
      ui_->groupBox->setTitle(QString());
   }

   
   addKey();

   if (flags_ & HideWidgetContol) {
      ui_->widgetControl->hide();
   }
}

void WalletKeysCreateWidget::addKey()
{
   assert(!walletInfo_.rootId().isEmpty());
   auto widget = new WalletKeyWidget(requestType_, walletInfo_, widgets_.size(), logger_, appSettings_, connectionManager_, this);
   widget->setUseType(useType_);


   if (flags_ & HidePubKeyFingerprint || true) {
      ui_->labelPubKeyFP->hide();
   }
   else {
      // TODO: Public key fingerprints need replacement
      //const auto &pubKeyFP = autheid::toHexWithSeparators(autheid::getPublicKeyFingerprint(authKeys.second));
      //ui_->labelPubKeyFP->setText(QString::fromStdString(pubKeyFP));
   }

   connect(widget, &WalletKeyWidget::passwordDataChanged, this, &WalletKeysCreateWidget::onPasswordDataChanged);
   connect(widget, &WalletKeyWidget::failed, this, &WalletKeysCreateWidget::failed);

   // propagate focus to next widget
   connect(widget, &WalletKeyWidget::returnPressed, this, [this](int keyIndex){
      if (widgets_.size() > keyIndex + 1)
         widgets_.at(keyIndex + 1)->setFocus();
      else
         emit returnPressed();
   });

   layout()->addWidget(widget);
   ui_->pushButtonDelKey->setEnabled(true);
   widgets_.emplace_back(widget);
   pwdData_.push_back(bs::wallet::PasswordData());
   ui_->spinBoxRankM->setMaximum(pwdData_.size());
   ui_->spinBoxRankM->setMinimum(1);
   updateKeyRank(0);
   emit keyCountChanged();
}

void WalletKeysCreateWidget::onAddClicked()
{
   addKey();
}

void WalletKeysCreateWidget::onDelClicked()
{
   if (widgets_.empty()) {
      return;
   }
   widgets_.pop_back();
   pwdData_.resize(widgets_.size());
   if (pwdData_.empty()) {
      ui_->spinBoxRankM->setMinimum(0);
   }
   ui_->spinBoxRankM->setMaximum(pwdData_.size());
   updateKeyRank(0);
   emit keyCountChanged();
   if (widgets_.empty()) {
      ui_->pushButtonDelKey->setEnabled(false);
   }
}

void WalletKeysCreateWidget::onPasswordDataChanged(int index, bs::wallet::PasswordData passwordData)
{
   pwdData_[index] = passwordData;
   emit keyChanged();
}

void WalletKeysCreateWidget::updateKeyRank(int)
{
   keyRank_.second = pwdData_.size();
   keyRank_.first = ui_->spinBoxRankM->value();
   if (pwdData_.empty()) {
      ui_->labelRankN->clear();
   }
   else {
      ui_->labelRankN->setText(QString::number(keyRank_.second));
   }
}

bool WalletKeysCreateWidget::isValid() const
{
   if (pwdData_.empty()) {
      return false;
   }

   std::set<SecureBinaryData> encKeys;
   for (const auto &pwd : pwdData_) {
      if (pwd.encType == bs::wallet::EncryptionType::Auth) {
         if (pwd.encKey.isNull()) {
            return false;
         }
         if (encKeys.find(pwd.encKey) != encKeys.end()) {
            return false;
         }
         encKeys.insert(pwd.encKey);
      } else if (pwd.password.getSize() < 6) {
         // Password must be at least 6 chars long.
         return false;
      }
   }
   return true;
}

void WalletKeysCreateWidget::setFocus()
{
   if (widgets_.empty()) {
      return;
   }
   widgets_.front()->setFocus();
}

void WalletKeysCreateWidget::cancel()
{
   for (const auto &keyWidget : widgets_) {
      keyWidget->cancel();
   }
}
