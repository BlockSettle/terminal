#ifndef __CELER_ACCOUNT_INFO_DIALOG_H__
#define __CELER_ACCOUNT_INFO_DIALOG_H__

#include <QDialog>
#include <memory>

namespace Ui {
    class CelerAccountInfoDialog;
};
class BaseCelerClient;

class CelerAccountInfoDialog : public QDialog
{
Q_OBJECT

public:
   CelerAccountInfoDialog(std::shared_ptr<BaseCelerClient> celerConnection, QWidget* parent = nullptr );
   ~CelerAccountInfoDialog() override;

private:
   std::unique_ptr<Ui::CelerAccountInfoDialog> ui_;
};

#endif // __CELER_ACCOUNT_INFO_DIALOG_H__
