#ifndef SIGNERS_MANAGE_WIDGET_H
#define SIGNERS_MANAGE_WIDGET_H

#include <QWidget>
#include <ApplicationSettings.h>

#include "SignersModel.h"

namespace Ui {
class SignerKeysWidget;
}

class SignerKeysWidget : public QWidget
{
   Q_OBJECT

public:
   explicit SignerKeysWidget(const std::shared_ptr<SignersProvider> &signersProvider
      , const std::shared_ptr<ApplicationSettings> &appSettings, QWidget *parent = nullptr);
   ~SignerKeysWidget();

public slots:
   void onAddSignerKey();
   void onDeleteSignerKey();
   void onEdit();
   void onSave();
   void onSelect();
   void onKeyImport();

signals:
   void needClose();

private slots:
   void resetForm();

private:
   std::unique_ptr<Ui::SignerKeysWidget> ui_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<SignersProvider> signersProvider_;

   SignersModel *signersModel_;
};

#endif // SIGNERS_MANAGE_WIDGET_H
