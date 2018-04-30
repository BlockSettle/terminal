#ifndef __SET_PASSPHRASE_FRAME_H__
#define __SET_PASSPHRASE_FRAME_H__

#include <QWizardPage>

namespace Ui {
    class SetPassphraseFrame;
};

class SetPassphraseFrame : public QWizardPage
{
Q_OBJECT

public:
   SetPassphraseFrame(QWidget* parent = nullptr );
   virtual ~SetPassphraseFrame();

   virtual bool isComplete() const override;

private:
   Ui::SetPassphraseFrame* ui_;
};

#endif // __SET_PASSPHRASE_FRAME_H__
