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
   ~SetPassphraseFrame() override;

   virtual bool isComplete() const override;

private:
   std::unique_ptr<Ui::SetPassphraseFrame> ui_;
};

#endif // __SET_PASSPHRASE_FRAME_H__
