#ifndef __NEWWALLETSEEDDIALOG_H__
#define __NEWWALLETSEEDDIALOG_H__

#include <memory>

#include <QDialog>

#include "EasyCoDec.h"

namespace Ui {
   class NewWalletSeedDialog;
}

class EasyEncValidator;
class WalletBackupPdfWriter;

class NewWalletSeedDialog : public QDialog
{
   Q_OBJECT

public:
   enum class Pages {
      PrintPreview,
      Confirm,
   };

   NewWalletSeedDialog(const QString& walletId
      , const QString &keyLine1, const QString &keyLine2, QWidget *parent = nullptr);
   ~NewWalletSeedDialog() override;

protected:
   void resizeEvent(QResizeEvent *e) override;

private slots:
   void reject() override;

   void onSaveClicked();
   void onPrintClicked();
   void onContinueClicked();
   void onBackClicked();

private:
   void setCurrentPage(Pages page);
   void updateState();
   void validateKeys();
   void onKeyChanged(const QString &);

   std::unique_ptr<Ui::NewWalletSeedDialog> ui_;
   std::unique_ptr<WalletBackupPdfWriter> pdfWriter_;
   Pages currentPage_;
   QSize sizePreview_;
   bool keysAreCorrect_ = false;
   const QString walletId_;
   const QString keyLine1_;
   const QString keyLine2_;
   std::shared_ptr<EasyCoDec> easyCodec_;
   std::unique_ptr<EasyEncValidator> validator_;
};

bool abortWalletCreationQuestionDialog(QWidget* parent);

#endif // __NEWWALLETSEEDDIALOG_H__
