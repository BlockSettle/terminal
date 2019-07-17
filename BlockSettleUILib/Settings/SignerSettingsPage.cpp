#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QClipboard>

#include "SignerSettingsPage.h"
#include "ui_SignerSettingsPage.h"
#include "ApplicationSettings.h"
#include "BtcUtils.h"
#include "BSMessageBox.h"
#include "SignContainer.h"
#include "SignersManageWidget.h"
#include "HeadlessContainer.h"

SignerSettingsPage::SignerSettingsPage(QWidget* parent)
   : SettingsPage{parent}
   , ui_{new Ui::SignerSettingsPage{}}
   , reset_{false}
{
   ui_->setupUi(this);
   ui_->widgetTwoWayAuth->hide();
   ui_->checkBoxTwoWayAuth->hide();

   //connect(ui_->comboBoxRunMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SignerSettingsPage::runModeChanged);
   connect(ui_->spinBoxAsSpendLimit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SignerSettingsPage::onAsSpendLimitChanged);
   connect(ui_->pushButtonManageSignerKeys, &QPushButton::clicked, this, &SignerSettingsPage::onManageSignerKeys);

   connect(ui_->pushButtonTerminalKeyCopy, &QPushButton::clicked, this, [this](){
      qApp->clipboard()->setText(ui_->labelTerminalKey->text());
      ui_->pushButtonTerminalKeyCopy->setEnabled(false);
      ui_->pushButtonTerminalKeyCopy->setText(tr("Copied"));
      QTimer::singleShot(2000, this, [this](){
         ui_->pushButtonTerminalKeyCopy->setEnabled(true);
         ui_->pushButtonTerminalKeyCopy->setText(tr("Copy"));
      });
   });
   connect(ui_->pushButtonTerminalKeySave, &QPushButton::clicked, this, [this](){
      QString fileName = QFileDialog::getSaveFileName(this
                                   , tr("Save Armory Public Key")
                                   , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + QStringLiteral("Terminal_Public_Key.pub")
                                   , tr("Key files (*.pub)"));

      QFile file(fileName);
      if (file.open(QIODevice::WriteOnly)) {
         file.write(ui_->labelTerminalKey->text().toLatin1());
      }
   });
}

SignerSettingsPage::~SignerSettingsPage() = default;

void SignerSettingsPage::display()
{
   ui_->checkBoxTwoWayAuth->setChecked(appSettings_->get<bool>(ApplicationSettings::twoWaySignerAuth));
   ui_->comboBoxSigner->setCurrentIndex(signersProvider_->indexOfCurrent());

   showLimits(signersProvider_->indexOfCurrent() == 0);
   ui_->labelTerminalKey->setText(QString::fromStdString(signersProvider_->remoteSignerOwnKey().toHexStr()));
}

void SignerSettingsPage::reset()
{
   reset_ = true;
   for (const auto &setting : {ApplicationSettings::localSignerPort
      , ApplicationSettings::remoteSigners, ApplicationSettings::autoSignSpendLimit
      , ApplicationSettings::twoWaySignerAuth}) {
      appSettings_->reset(setting, false);
   }
   display();
   reset_ = false;
}

void SignerSettingsPage::showHost(bool show)
{
   ui_->labelHost->setVisible(true);
   ui_->labelHost_2->setVisible(true);
   ui_->comboBoxSigner->setVisible(true);
}

void SignerSettingsPage::showLimits(bool show)
{
   ui_->groupBoxAutoSign->setEnabled(show);
   ui_->labelAsSpendLimit->setEnabled(show);
   ui_->spinBoxAsSpendLimit->setEnabled(show);
   onAsSpendLimitChanged(ui_->spinBoxAsSpendLimit->value());
}

void SignerSettingsPage::showSignerKeySettings(bool show)
{
   //ui_->widgetTwoWayAuth->setVisible(show);
   //ui_->checkBoxTwoWayAuth->setVisible(show);
   ui_->widgetSignerKeyComboBox->setVisible(show);
   ui_->SignerDetailsGroupBox->setVisible(show);
}

void SignerSettingsPage::onAsSpendLimitChanged(double value)
{
   if (value > 0) {
      ui_->labelAsSpendLimit->setText(tr("Spend Limit:"));
   }
   else {
      ui_->labelAsSpendLimit->setText(tr("Spend Limit - unlimited"));
   }
}

void SignerSettingsPage::onManageSignerKeys()
{
   // workaround here - wrap widget by QDialog
   // TODO: fix stylesheet to support popup widgets

   QDialog *d = new QDialog(this);
   QVBoxLayout *l = new QVBoxLayout(d);
   l->setContentsMargins(0,0,0,0);
   d->setLayout(l);
   d->setWindowTitle(tr("Manage Signer Connection"));

   SignerKeysWidget *signerKeysWidget = new SignerKeysWidget(signersProvider_, appSettings_, this);
   d->resize(signerKeysWidget->size());

   l->addWidget(signerKeysWidget);

   connect(signerKeysWidget, &SignerKeysWidget::needClose, this, [d](){
      d->reject();
   });

   d->exec();

   emit signersChanged();
}

void SignerSettingsPage::apply()
{
   appSettings_->set(ApplicationSettings::twoWaySignerAuth, ui_->checkBoxTwoWayAuth->isChecked());
   signersProvider_->setupSigner(ui_->comboBoxSigner->currentIndex());
}

void SignerSettingsPage::initSettings()
{
   signersModel_ = new SignersModel(signersProvider_, this);
   signersModel_->setSingleColumnMode(true);
   signersModel_->setHighLightSelectedServer(false);
   ui_->comboBoxSigner->setModel(signersModel_);

   connect(signersProvider_.get(), &SignersProvider::dataChanged, this, &SignerSettingsPage::display);
}

void SignerSettingsPage::init(const std::shared_ptr<ApplicationSettings> &appSettings
                              , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
                              , const std::shared_ptr<SignersProvider> &signersProvider
                              , std::shared_ptr<SignContainer> signContainer)
{
   reset_ = true;
   SettingsPage::init(appSettings, armoryServersProvider, signersProvider, signContainer);
   reset_ = false;
}
