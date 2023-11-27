/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
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

class WalletBackupPdfWriter final
{
public:
   static constexpr int kResolution = 1200;
   static constexpr double kTotalWidthInches = 8.27;
   static constexpr double kTotalHeightInches = 11.69;
   static constexpr double kMarginInches = 0.0;

public:
   WalletBackupPdfWriter(const QString &walletId
      , const QStringList& seed
      , const QPixmap &qr);

   bool write(const QString &fileName);

   QPixmap getPreview(int width, double marginScale);

   void print(QPrinter *printer);
   void draw(QPainter &p, qreal width, qreal height);

private:
   QString walletId_;
   QStringList seed_;
   QPixmap qr_;
};

#endif // PAPERBACKUPWRITER_H_INCLUDED
