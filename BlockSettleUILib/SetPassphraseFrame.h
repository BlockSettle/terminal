/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SET_PASSPHRASE_FRAME_H__
#define __SET_PASSPHRASE_FRAME_H__

#include <QWizardPage>

#include <memory>

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
