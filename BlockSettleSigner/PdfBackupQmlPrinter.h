
#ifndef PDFBACKUPQMLPRINTER_H_INCLUDED
#define PDFBACKUPQMLPRINTER_H_INCLUDED

#include <QQuickPaintedItem>

#include "PaperBackupWriter.h"
#include "QWalletInfo.h"
#include "QSeed.h"
#include "QPasswordData.h"
#include <memory>

//
// QmlPdfBackup
//

//! QML item to render PDF backup.
class QmlPdfBackup : public QQuickPaintedItem
{
   Q_OBJECT

   Q_PROPERTY(bs::wallet::QSeed* seed READ seed WRITE setSeed NOTIFY seedChanged)
   Q_PROPERTY(qreal preferedHeight READ preferedHeight NOTIFY preferedHeightChanged)

signals:
   void preferedHeightChanged();

public:
   QmlPdfBackup(QQuickItem *parent = nullptr);
   ~QmlPdfBackup() noexcept override = default;

   void paint(QPainter *painter) override;
   void componentComplete() override;

   qreal preferedHeight() const;

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
