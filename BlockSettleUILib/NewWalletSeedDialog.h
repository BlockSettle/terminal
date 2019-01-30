#ifndef __NEWWALLETSEEDDIALOG_H__
#define __NEWWALLETSEEDDIALOG_H__

#include <memory>

#include <QDialog>

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
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
   void reject() override;
   void onSaveClicked();
   void onPrintClicked();

private:
   std::unique_ptr<Ui::NewWalletSeedDialog> ui_;
   std::unique_ptr<WalletBackupPdfWriter> pdfWriter_;
   Pages currentPage_;
   QSize sizePreview_;
   bool keysAreCorrect_ = false;
   const QString walletId_;
   const QString keyLine1_;
   const QString keyLine2_;
};

#endif // __NEWWALLETSEEDDIALOG_H__
