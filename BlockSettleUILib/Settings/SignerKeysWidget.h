#ifndef SIGNER_KEYS_WIDGET_H
#define SIGNER_KEYS_WIDGET_H

#include <QWidget>
#include <ApplicationSettings.h>

#include "SignerKeysModel.h"

namespace Ui {
class SignerKeysWidget;
}

class SignerKeysWidget : public QWidget
{
   Q_OBJECT

public:
   explicit SignerKeysWidget(std::shared_ptr<ApplicationSettings> appSettings, QWidget *parent = nullptr);
   ~SignerKeysWidget();

public slots:
   void onAddSignerKey();
   void onDeleteSignerKey();
   void onEdit();
   void onSave();

signals:
   void reconnectArmory();
   void needClose();

private slots:
   void resetForm();

private:
   std::unique_ptr<Ui::SignerKeysWidget> ui_;
   std::shared_ptr<ApplicationSettings> appSettings_;

   SignerKeysModel *signerKeysModel_;
};

#endif // SIGNER_KEYS_WIDGET_H
