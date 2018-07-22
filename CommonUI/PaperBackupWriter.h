
#ifndef PAPERBACKUPWRITER_H_INCLUDED
#define PAPERBACKUPWRITER_H_INCLUDED

#include <QString>
#include <QPixmap>


//
// WalletBackupPdfWriter
//

//! Writer of PDF backup of wallet.
class WalletBackupPdfWriter final
{
public:
   WalletBackupPdfWriter(const QString &walletName,
      const QString &walletId, const QString &keyLine1, const QString &keyLine2,
      const QPixmap &logo, const QPixmap &qr);

   //! Write backup to PDF.
   bool write(const QString &fileName);

private:
   //! Draw backup.
   void draw(QPainter &p, qreal width, qreal height);

private:
   QString walletName_;
   QString walletId_;
   QString keyLine1_;
   QString keyLine2_;
   QPixmap logo_;
   QPixmap qr_;
}; // class WalletBackupPdfWriter

#endif // PAPERBACKUPWRITER_H_INCLUDED
