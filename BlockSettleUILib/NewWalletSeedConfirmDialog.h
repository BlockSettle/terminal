#ifndef __NEWWALLETSEEDCONFIRMDIALOG_H__
#define __NEWWALLETSEEDCONFIRMDIALOG_H__

#include <memory>

#include <QDialog>

#include "BtcDefinitions.h"
#include "EasyCoDec.h"

namespace Ui {
   class NewWalletSeedConfirmDialog;
}

class EasyEncValidator;
class WalletBackupPdfWriter;

class NewWalletSeedConfirmDialog : public QDialog
{
   Q_OBJECT

public:
   NewWalletSeedConfirmDialog(const std::string &walletId, NetworkType
      , const QString &keyLine1, const QString &keyLine2, QWidget *parent = nullptr);
   ~NewWalletSeedConfirmDialog() override;

protected:
   void updateState();

private slots:
   void reject() override;

   void onContinueClicked();
   void onBackClicked();

private:
   void validateKeys();
   void onKeyChanged(const QString &);

   std::unique_ptr<Ui::NewWalletSeedConfirmDialog> ui_;
   bool keysAreCorrect_ = false;
   const std::string walletId_;
   const NetworkType netType_;
   const QString keyLine1_;
   const QString keyLine2_;
   std::shared_ptr<EasyCoDec> easyCodec_;
   std::unique_ptr<EasyEncValidator> validator_;
};

#endif // __NEWWALLETSEEDCONFIRMDIALOG_H__
