#include "LoginWindow.h"
#include "ui_LoginWindow.h"
#include "AboutDialog.h"
#include "ApplicationSettings.h"
#include "UiUtils.h"

#include <QSettings>
#include <QIcon>
#include <spdlog/spdlog.h>

LoginWindow::LoginWindow(const std::shared_ptr<ApplicationSettings> &settings, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::LoginWindow())
   , settings_(settings)
   , autheID_(false)
{
   ui_->setupUi(this);
   const auto version = ui_->loginVersionLabel->text().replace(QLatin1String("{Version}")
      , tr("Version %1").arg(QString::fromStdString(AboutDialog::version())));
   ui_->loginVersionLabel->setText(version);
   resize(minimumSize());


   const auto accountLink = ui_->labelGetAccount->text().replace(QLatin1String("{GetAccountLink}")
      , settings->get<QString>(ApplicationSettings::GetAccount_Url));
   ui_->labelGetAccount->setText(accountLink);

   connect(ui_->lineEditUsername, &QLineEdit::textChanged, this, &LoginWindow::onTextChanged);

   const QString username = settings_->get<QString>(ApplicationSettings::celerUsername);
   if (!username.isEmpty()) {
      ui_->lineEditUsername->setText(username);
   }
   else {
      ui_->lineEditUsername->setFocus();
   }

   connect(ui_->signWithEidButton, &QPushButton::clicked, this, &LoginWindow::onAuthPressed);
}

LoginWindow::~LoginWindow() = default;

void LoginWindow::onTextChanged()
{
   ui_->signWithEidButton->setEnabled(!ui_->lineEditUsername->text().isEmpty());
}

QString LoginWindow::getUsername() const
{
   return ui_->lineEditUsername->text().toLower();
}

void LoginWindow::onAuthPressed()
{
   autheID_ = true;
   if (ui_->checkBoxRememberUsername->isChecked()) {
      settings_->set(ApplicationSettings::celerUsername, ui_->lineEditUsername->text());
   }
   accept();
   ui_->signWithEidButton->setEnabled(false);
}

void LoginWindow::onAuthSucceeded(const QString &userId, const QString &details)
{
   auto palette = ui_->signWithEidButton->palette();
   palette.setColor(QPalette::Button, QColor(Qt::green));
   ui_->signWithEidButton->setAutoFillBackground(true);
   ui_->signWithEidButton->setPalette(palette);
   ui_->signWithEidButton->update();
   ui_->signWithEidButton->setText(tr("Successfully authenticated"));
}

void LoginWindow::onAuthFailed(const QString &userId, const QString &text)
{
   auto palette = ui_->signWithEidButton->palette();
   palette.setColor(QPalette::Button, QColor(Qt::red));
   ui_->signWithEidButton->setAutoFillBackground(true);
   ui_->signWithEidButton->setPalette(palette);
   ui_->signWithEidButton->update();
   ui_->signWithEidButton->setText(tr("Auth auth failed: %1").arg(text));
}

void LoginWindow::onAuthStatusUpdated(const QString &userId, const QString &status)
{
   ui_->signWithEidButton->setText(status);
}
