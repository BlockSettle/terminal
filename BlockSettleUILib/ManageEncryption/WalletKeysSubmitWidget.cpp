#include "WalletKeysSubmitWidget.h"
#include "ui_WalletKeysSubmitWidget.h"
#include <set>
#include <QFrame>
#include <QtConcurrent/QtConcurrentRun>
#include "ApplicationSettings.h"

using namespace bs::wallet;
using namespace bs::hd;


WalletKeysSubmitWidget::WalletKeysSubmitWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::WalletKeysSubmitWidget())
   , suspended_(false)
{
   ui_->setupUi(this);

   // currently we dont using m of n
   ui_->groupBox->hide();
   layout()->removeWidget(ui_->groupBox);
}

WalletKeysSubmitWidget::~WalletKeysSubmitWidget() = default;

void WalletKeysSubmitWidget::setFlags(Flags flags)
{
   flags_ = flags;
}

void WalletKeysSubmitWidget::init(AutheIDClient::RequestType requestType
                                     , const bs::hd::WalletInfo &walletInfo
                                     , WalletKeyWidget::UseType useType
                                     , const std::shared_ptr<spdlog::logger> &logger
                                     , const std::shared_ptr<ApplicationSettings> &appSettings
                                     , const std::shared_ptr<ConnectionManager> &connectionManager
                                     , const QString &prompt)
{
   requestType_ = requestType;
   walletInfo_ = walletInfo;
   logger_ = logger;
   appSettings_ = appSettings;
   connectionManager_ = connectionManager;
   useType_ = useType;

   qDeleteAll(widgets_.cbegin(), widgets_.cend());
   widgets_.clear();
   pwdData_.clear();

   if (flags_ & HideGroupboxCaption) {
      ui_->groupBox->setTitle(QString());
   }

   if (walletInfo.encTypes().empty()) {
      return;
   }

   bool hasAuth = false;
   for (const auto &encType : walletInfo.encTypes()) {
      if (encType == EncryptionType::Auth) {
         hasAuth = true;
         break;
      }
   }
   if ((flags_ & HidePubKeyFingerprint) || !hasAuth || true) {
      ui_->labelPubKeyFP->hide();
   }
   else {
      ui_->labelPubKeyFP->show();
      // TODO: Public key fingerprints need replacement
//      QtConcurrent::run([this] {
//         const auto &authKeys = appSettings_->GetAuthKeys();
//         const auto &pubKeyFP = autheid::getPublicKeyFingerprint(authKeys.second);
//         const auto &sPubKeyFP = QString::fromStdString(autheid::toHexWithSeparators(pubKeyFP));
//         QMetaObject::invokeMethod(this, [this, sPubKeyFP] {
//            ui_->labelPubKeyFP->setText(sPubKeyFP);
//         });
//      });
   }


   int encKeyIndex = 0;
   if (walletInfo.isEidAuthOnly()) {
      addKey(0, prompt);
   }
   else if (walletInfo.encTypes().size() == walletInfo.keyRank().first) {
      for (const auto &encType : walletInfo.encTypes()) {
         const bool isPassword = (encType == EncryptionType::Password);
         addKey(isPassword ? 0 : encKeyIndex++, prompt);
      }
   }
   else {
      if ((walletInfo.encTypes().size() > 1) && (walletInfo.keyRank().first == 1)) {
         addKey(0, prompt);
      }
      else {
         if ((walletInfo.encTypes().size() == 1) && (walletInfo.encTypes()[0] == EncryptionType::Auth)
             && (walletInfo.encKeys().size() == walletInfo.keyRank().first)) {
            for (unsigned int i = 0; i < walletInfo.keyRank().first; ++i) {
               addKey(encKeyIndex++, prompt);
            }
         }
         else if ((walletInfo.encTypes().size() == 1) && (walletInfo.encTypes()[0] == EncryptionType::Password)) {
            for (unsigned int i = 0; i < walletInfo.keyRank().first; ++i) {
               addKey(0, prompt);
            }
         }
         else {
            for (unsigned int i = 0; i < walletInfo.keyRank().first; ++i) {
               const bool isPassword = !(encKeyIndex < walletInfo.encKeys().size());
               addKey(isPassword ? 0 : encKeyIndex++, prompt);
            }
         }
      }
   }
}


void WalletKeysSubmitWidget::addKey(int encKeyIndex, const QString &prompt)
{
   assert(!walletInfo_.rootId().isEmpty());
   if (!widgets_.empty()) {
      const auto separator = new QFrame(this);
      separator->setFrameShape(QFrame::HLine);
      layout()->addWidget(separator);
   }

   auto widget = new WalletKeyWidget(requestType_, walletInfo_, encKeyIndex
      , logger_, appSettings_, connectionManager_, this);
   widget->setUseType(useType_);

   widgets_.push_back(widget);
   pwdData_.push_back(PasswordData());


   connect(widget, &WalletKeyWidget::passwordDataChanged, this, &WalletKeysSubmitWidget::onPasswordDataChanged);
   connect(widget, &WalletKeyWidget::failed, this, &WalletKeysSubmitWidget::failed);

   // propagate focus to next widget
   connect(widget, &WalletKeyWidget::returnPressed, this, [this](int keyIndex){
      if (widgets_.size() > keyIndex + 1)
         widgets_.at(keyIndex + 1)->setFocus();
      else
         emit returnPressed();
   });


   layout()->addWidget(widget);

   emit keyCountChanged();
}

void WalletKeysSubmitWidget::setFocus()
{
   if (widgets_.empty()) {
      return;
   }
   widgets_.front()->setFocus();
}

void WalletKeysSubmitWidget::onPasswordDataChanged(int index, PasswordData passwordData)
{
   pwdData_[index] = passwordData;
   emit keyChanged();
}


bool WalletKeysSubmitWidget::isValid() const
{
   if (pwdData_.empty()) {
      return true;
   }
   std::set<SecureBinaryData> encKeys;
   for (const auto &pwd : pwdData_) {
      if (pwd.password.isNull()) {
         return false;
      }
      if (pwd.encType == bs::wallet::EncryptionType::Auth) {
         if (pwd.encKey.isNull()) {
            return false;
         }
         if (encKeys.find(pwd.encKey) != encKeys.end()) {
            return false;
         }
         encKeys.insert(pwd.encKey);
      }
   }
   return true;
}

void WalletKeysSubmitWidget::cancel()
{
   for (const auto &keyWidget : widgets_) {
      keyWidget->cancel();
   }
}

SecureBinaryData WalletKeysSubmitWidget::key() const
{
   SecureBinaryData result;
   for (const auto &pwd : pwdData_) {
      result = mergeKeys(result, pwd.password);
   }
   return result;
}

bool WalletKeysSubmitWidget::isKeyFinal() const
{
   return isKeyFinal_;
}

void WalletKeysSubmitWidget::resume()
{
   suspended_ = false;
   for (const auto &widget : widgets_) {
      widget->start();
   }
}



