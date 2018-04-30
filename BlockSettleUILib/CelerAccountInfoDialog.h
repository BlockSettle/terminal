#ifndef __CELER_ACCOUNT_INFO_DIALOG_H__
#define __CELER_ACCOUNT_INFO_DIALOG_H__

#include <QDialog>
#include <memory>

namespace Ui {
    class CelerAccountInfoDialog;
};
class CelerClient;

class CelerAccountInfoDialog : public QDialog
{
Q_OBJECT

public:
   CelerAccountInfoDialog(std::shared_ptr<CelerClient> celerConnection, QWidget* parent = nullptr );
   ~CelerAccountInfoDialog() override = default;

private:
   Ui::CelerAccountInfoDialog* ui_;
};

#endif // __CELER_ACCOUNT_INFO_DIALOG_H__
