#ifndef __WALLETKEYSSUBMITTEST_H__
#define __WALLETKEYSSUBMITTEST_H__

#include <memory>
#include <QDialog>

namespace Ui {
class WalletKeysSubmitTest;
}
class ApplicationSettings;
class WalletKeysSubmitWidget;

class WalletKeysSubmitTest : public QDialog
{
   Q_OBJECT

public:
   explicit WalletKeysSubmitTest(const std::shared_ptr<ApplicationSettings> &appSettings
      , QWidget *parent = 0);
   ~WalletKeysSubmitTest() override;

private:
   enum Type
   {
      Password,
      Auth,
      AuthMultiple,
   };

   void addCheckbox(unsigned flag, const char *name);
   void update();
   void resetWidget(WalletKeysSubmitWidget* &testWidget, Type type);

   std::unique_ptr<Ui::WalletKeysSubmitTest> ui_;
   std::shared_ptr<ApplicationSettings> appSettings_;

   WalletKeysSubmitWidget *testWidget1_{};
   WalletKeysSubmitWidget *testWidget2_{};
   WalletKeysSubmitWidget *testWidget3_{};
   unsigned flags_{};
};

#endif // __WALLETKEYSSUBMITTEST_H__
