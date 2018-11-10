#include "WalletKeysSubmitTest.h"
#include "ui_WalletKeysSubmitTest.h"

#include <QCheckBox>
#include "WalletKeysSubmitWidget.h"


WalletKeysSubmitTest::WalletKeysSubmitTest(const std::shared_ptr<ApplicationSettings> &appSettings
   , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::WalletKeysSubmitTest)
   , appSettings_(appSettings)
{
   ui_->setupUi(this);

   addCheckbox(WalletKeysSubmitWidget::HideAuthConnectButton, "HideAuthConnectButton");
   addCheckbox(WalletKeysSubmitWidget::HideAuthCombobox, "HideAuthCombobox");
   addCheckbox(WalletKeysSubmitWidget::HideGroupboxCaption, "HideGroupboxCaption");
   addCheckbox(WalletKeysSubmitWidget::AuthProgressBarFixed, "AuthProgressBarFixed");
   addCheckbox(WalletKeysSubmitWidget::AuthIdVisible, "AuthIdVisible");
   addCheckbox(WalletKeysSubmitWidget::SetPasswordLabelAsOld, "SetPasswordLabelAsOld");
   addCheckbox(WalletKeysSubmitWidget::HideAuthEmailLabel, "HideAuthEmailLabel");
   addCheckbox(WalletKeysSubmitWidget::HideAuthControlsOnSignClicked, "HideAuthControlsOnSignClicked");
   addCheckbox(WalletKeysSubmitWidget::HidePubKeyFingerprint, "HidePubKeyFingerprint");
   addCheckbox(WalletKeysSubmitWidget::HideProgressBar, "HideProgressBar");
}

WalletKeysSubmitTest::~WalletKeysSubmitTest() = default;

void WalletKeysSubmitTest::addCheckbox(unsigned flag, const char *name)
{
   QCheckBox *checkBox = new QCheckBox(QLatin1String(name));

   connect(checkBox, &QCheckBox::clicked, [this, checkBox, flag] {
      if (checkBox->isChecked()) {
         flags_ |= flag;
      } else {
         flags_ &= ~flag;
      }
      update();
   });

   ui_->verticalLayout->insertWidget(0, checkBox);
}

void WalletKeysSubmitTest::update()
{
   resetWidget(testWidget1_, Password);
   resetWidget(testWidget2_, Auth);
   resetWidget(testWidget3_, AuthMultiple);
}

void WalletKeysSubmitTest::resetWidget(WalletKeysSubmitWidget* &testWidget, Type type)
{
   delete testWidget;
   testWidget = new WalletKeysSubmitWidget;
   testWidget->setFlags(WalletKeysSubmitWidget::Flags(flags_));

   bs::wallet::KeyRank keyRank = std::make_pair(1, 1);
   std::vector<bs::wallet::EncryptionType> encTypes;
   std::vector<SecureBinaryData> encKeys;

   if (type == Password) {
      encTypes.push_back(bs::wallet::EncryptionType::Password);
      encKeys.push_back({});
   } else if (type == Auth) {
      encTypes.push_back(bs::wallet::EncryptionType::Auth);
      encKeys.push_back(SecureBinaryData("t@t.t:AmbWx/XllvoOfo/qGjhjljsHPy5lr26v/Bwbx1YxP4kw:Test"));
   } else if (type == AuthMultiple) {
      keyRank.second = 3;
      encTypes.push_back(bs::wallet::EncryptionType::Auth);
      encKeys.push_back(SecureBinaryData("t@t.t:AmbWx/XllvoOfo/qGjhjljsHPy5lr26v/Bwbx1YxP4kw:Test1"));
      encKeys.push_back(SecureBinaryData("t@t.t:Ax0SY2RQqDQlIjP3LmrbBdjj1G+ZUgQfH6HlKTcW5FpH:Test2"));
      encKeys.push_back(SecureBinaryData("t@t.t:Aj4p773fwc65WtlIWN7dREzc7AP85I1IcMftOuffRASm:Test3"));
   }

   testWidget->init(MobileClientRequest::SignWallet, "25mBcVXsL", keyRank, encTypes, encKeys, appSettings_);

   testWidget->resume();

   ui_->verticalLayout->addWidget(testWidget);
}
