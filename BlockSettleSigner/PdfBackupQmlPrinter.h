/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef PDFBACKUPQMLPRINTER_H_INCLUDED
#define PDFBACKUPQMLPRINTER_H_INCLUDED

#include <QQuickPaintedItem>

#include "QSeed.h"
#include "PaperBackupWriter.h"
#include <memory>

namespace bs {
namespace wallet {
class QSeed;
} }
//
// QmlPdfBackup
//

//! QML item to render PDF backup.
class QmlPdfBackup : public QQuickPaintedItem
{
   Q_OBJECT

   Q_PROPERTY(bs::wallet::QSeed* seed READ seed WRITE setSeed NOTIFY seedChanged)
   Q_PROPERTY(qreal preferredHeightForWidth READ preferredHeightForWidth NOTIFY preferredHeightForWidthChanged)

signals:
   void preferredHeightForWidthChanged();

public:
   QmlPdfBackup(QQuickItem *parent = nullptr);
   ~QmlPdfBackup() noexcept override = default;

   void paint(QPainter *painter) override;
   void componentComplete() override;

   qreal preferredHeightForWidth() const;

   bs::wallet::QSeed *seed() const;
   void setSeed(bs::wallet::QSeed *seed);

   Q_INVOKABLE void save();
   Q_INVOKABLE void print();
private slots:
   void onWidthChanged();
   void onSeedChanged();

signals:
   void seedChanged();

   void printFailed();
   void printSucceed();

   void saveFailed(const QString &filePath);
   void saveSucceed(const QString &filePath);

private:
   bs::wallet::QSeed *seed_;
   std::unique_ptr<WalletBackupPdfWriter> pdf_;
   qreal kTotalHeightInches = 11;
   qreal kTotalWidthInches = 8.27;
   qreal kMarginInches = 1.0;
   qreal kResolution = 1200;
}; // class QmlPdfBackup

#endif // PDFBACKUPQMLPRINTER_H_INCLUDED
