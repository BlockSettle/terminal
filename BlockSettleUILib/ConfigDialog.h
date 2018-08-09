#ifndef __CONFIG_DIALOG_H__
#define __CONFIG_DIALOG_H__

#include <memory>
#include <QDialog>

class ApplicationSettings;
class AssetManager;
class WalletsManager;

namespace Ui {
   class ConfigDialog;
}

class ConfigDialog : public QDialog
{
Q_OBJECT

public:
   ConfigDialog(const std::shared_ptr<ApplicationSettings>& appSettings
      , const std::shared_ptr<WalletsManager>& walletsMgr
      , const std::shared_ptr<AssetManager> &assetMgr
      , QWidget* parent = nullptr);

private slots:
   void onDisplayDefault();
   void onAcceptSettings();
   void onSelectionChanged(int currentRow);
   void illformedSettings(bool illformed);

private:
   Ui::ConfigDialog *ui_;
   std::shared_ptr<ApplicationSettings>   applicationSettings_;
   std::shared_ptr<WalletsManager>        walletsMgr_;
   std::shared_ptr<AssetManager>          assetMgr_;
};

#endif
