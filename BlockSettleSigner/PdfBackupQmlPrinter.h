
#ifndef PDFBACKUPQMLPRINTER_H_INCLUDED
#define PDFBACKUPQMLPRINTER_H_INCLUDED

#include <QQuickPaintedItem>

#include "PaperBackupWriter.h"

#include <memory>


//
// QmlPdfBackup
//

//! QML item to render PDF backup.
class QmlPdfBackup : public QQuickPaintedItem
{
   Q_OBJECT

   Q_PROPERTY(QString walletId READ walletId WRITE setWalletId)
   Q_PROPERTY(QString part1 READ part1 WRITE setPart1)
   Q_PROPERTY(QString part2 READ part2 WRITE setPart2)
   Q_PROPERTY(qreal preferedHeight READ preferedHeight NOTIFY preferedHeightChanged)

signals:
   void preferedHeightChanged();

public:
   QmlPdfBackup(QQuickItem *parent = nullptr);
   ~QmlPdfBackup() noexcept override = default;

   void paint(QPainter *painter) override;

   const QString& walletId() const;
   void setWalletId(const QString &id);

   const QString& part1() const;
   void setPart1(const QString &p1);

   const QString& part2() const;
   void setPart2(const QString &p2);

   qreal preferedHeight() const;

private slots:
   void onWidthChanged();

private:
   QString walletId_;
   QString part1_;
   QString part2_;
   std::unique_ptr<WalletBackupPdfWriter> pdf_;
   qreal kTotalHeightInches = 11;
   qreal kTotalWidthInches = 8.27;
   qreal kMarginInches = 1.0;
   qreal kResolution = 1200;
}; // class QmlPdfBackup

#endif // PDFBACKUPQMLPRINTER_H_INCLUDED
