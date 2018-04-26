#ifndef __WALLET_CREATE_COMPLETE_DIALOG_H__
#define __WALLET_CREATE_COMPLETE_DIALOG_H__

#include <QDialog>

namespace Ui
{
   class WalletCompleteDialog;
};

class WalletCompleteDialog : public QDialog
{
Q_OBJECT

public:
   WalletCompleteDialog(const QString& walletName, bool asPrimary, QWidget* parent = nullptr);
   ~WalletCompleteDialog() noexcept override = default;

   int exec();

protected:
   virtual QString infoText() const = 0;
   virtual QString titleText() const = 0;

   const QString  walletName_;
   const bool     primary_;
   const QString  primaryPrefix_;

private:
   Ui::WalletCompleteDialog * ui_;
};


class WalletCreateCompleteDialog : public WalletCompleteDialog
{
   Q_OBJECT

public:
   WalletCreateCompleteDialog(const QString& walletName, bool asPrimary, QWidget* parent = nullptr)
      : WalletCompleteDialog(walletName, asPrimary, parent) {}
   ~WalletCreateCompleteDialog() noexcept override = default;

protected:
   QString infoText() const override;
   QString titleText() const override;
};


class WalletImportCompleteDialog : public WalletCompleteDialog
{
   Q_OBJECT

public:
   WalletImportCompleteDialog(const QString& walletName, bool asPrimary, QWidget* parent = nullptr)
      : WalletCompleteDialog(walletName, asPrimary, parent) {}
   ~WalletImportCompleteDialog() noexcept override = default;

protected:
   QString infoText() const override;
   QString titleText() const override;
};

#endif // __WALLET_CREATE_COMPLETE_DIALOG_H__
