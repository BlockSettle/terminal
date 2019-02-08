#ifndef __WALLETKEYSDELETEDEVICE_H__
#define __WALLETKEYSDELETEDEVICE_H__

#include <memory>
#include <QWidget>

namespace Ui {
class WalletKeysDeleteDevice;
}

class WalletKeysDeleteDevice : public QWidget
{
   Q_OBJECT

public:
   explicit WalletKeysDeleteDevice(const QString &deviceName, QWidget *parent = 0);
   ~WalletKeysDeleteDevice();

signals:
   void deleteClicked();

private:
   std::unique_ptr<Ui::WalletKeysDeleteDevice> ui_;
};

#endif // __WALLETKEYSDELETEDEVICE_H__
