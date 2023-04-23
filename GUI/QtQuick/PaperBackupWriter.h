/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#ifndef PAPERBACKUPWRITER_H_INCLUDED
#define PAPERBACKUPWRITER_H_INCLUDED

#include <QString>
#include <QPixmap>

QT_BEGIN_NAMESPACE
class QPrinter;
QT_END_NAMESPACE

//
// WalletBackupPdfWriter
//

//! Writer of PDF backup of wallet.
class WalletBackupPdfWriter final
{
public:
   static constexpr int kResolution = 1200;
   static constexpr double kTotalWidthInches = 8.27;
   static constexpr double kTotalHeightInches = 11.69;
   static constexpr double kMarginInches = 0.0;

public:
   WalletBackupPdfWriter(const QString &walletId
      , const QString &keyLine1, const QString &keyLine2
      , const QPixmap &qr);

   //! Write backup to PDF.
   bool write(const QString &fileName);

   //! Generate resulted PDF preview pixmap. Height will be calculated using page aspect ratio.
   // Use marginScale to lower visible margins (valid range 0..1).
   QPixmap getPreview(int width, double marginScale);

   //! Print on selected printer using printer's page size (A4 and letter only supported at the moment).
   void print(QPrinter *printer);
   //! Draw backup.
   void draw(QPainter &p, qreal width, qreal height);

private:
   QString walletId_;
   QString keyLine1_;
   QString keyLine2_;
   QPixmap qr_;
}; // class WalletBackupPdfWriter

#endif // PAPERBACKUPWRITER_H_INCLUDED
