#ifndef __IMPORT_TYPE_WALLET_H__
#define __IMPORT_TYPE_WALLET_H__

#include <memory>
#include <QDialog>

#include "EasyCoDec.h"
#include "WalletBackupFile.h"

namespace Ui {
    class ImportWalletTypeDialog;
}
class EasyEncValidator;


class ImportWalletTypeDialog : public QDialog
{
Q_OBJECT

public:
   enum ImportType {
      Full,
      WatchingOnly
   };

   ImportWalletTypeDialog(QWidget* parent = nullptr );
   ~ImportWalletTypeDialog() override;

   EasyCoDec::Data GetSeedData() const;
   EasyCoDec::Data GetChainCodeData() const;
   std::string GetName() const;
   std::string GetDescription() const;
   QString GetWatchinOnlyFileName() const { return woFileName_; }
   ImportType type() const { return importType_; }

private slots:
   void OnTabTypeChanged(int index);
   void OnPaperSelected();
   void OnDigitalSelected();

   void updateImportButton();

   void OnSelectFilePressed();
   void OnSelectWoFilePressed();

private:
   bool PaperImportOk();
   bool DigitalImportOk();
   bool WoImportOk() const { return woFileExists_; }

protected:
   void resizeEvent(QResizeEvent* event) override;

private:
   Ui::ImportWalletTypeDialog* ui_;

   QString                       digitalBackupFile_;
   WalletBackupFile::WalletData  walletData_;
   bool        woFileExists_ = false;
   QString     woFileName_;
   ImportType  importType_ = Full;
   std::shared_ptr<EasyCoDec> easyCodec_;
   EasyEncValidator         * validator_ = nullptr;
};


#endif // __IMPORT_TYPE_WALLET_H__
