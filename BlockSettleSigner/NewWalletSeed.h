
#ifndef NEWWALLETSEED_H_INCLUDED
#define NEWWALLETSEED_H_INCLUDED

#include <QObject>

#include <memory>

#include "SignerSettings.h"
#include "PaperBackupWriter.h"


//
// NewWalletSeed
//

//! Helper to generate new wallet seed.
class NewWalletSeed : public QObject
{
   Q_OBJECT

   Q_PROPERTY(QString walletId READ walletId NOTIFY walletIdChanged)
   Q_PROPERTY(QString part1 READ part1 NOTIFY part1Changed)
   Q_PROPERTY(QString part2 READ part2 NOTIFY part2Changed)

signals:
   void unableToPrint();
   void walletIdChanged();
   void part1Changed();
   void part2Changed();

public:
   NewWalletSeed(std::shared_ptr<SignerSettings> settings, QObject *parent);
   ~NewWalletSeed() noexcept override = default;

   const QString& walletId() const;
   const QString& part1() const;
   const QString& part2() const;

public slots:
   void generate();
   void print();
   void save(const QString &fileName);

private:
   QString walletId_;
   QString part1_;
   QString part2_;
   std::shared_ptr<SignerSettings> settings_;
   std::unique_ptr<WalletBackupPdfWriter> pdfWriter_;
}; // class NewWalletSeed

#endif // NEWWALLETSEED_H_INCLUDED
