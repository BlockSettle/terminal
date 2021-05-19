/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SIGNER_SETTINGS_PAGE_H__
#define __SIGNER_SETTINGS_PAGE_H__

#include <memory>
#include "ConfigDialog.h"
#include "SignersModel.h"

namespace Ui {
   class SignerSettingsPage;
}
class ApplicationSettings;
class SignerKeysWidget;


class SignerSettingsPage : public SettingsPage
{
   Q_OBJECT
public:
   SignerSettingsPage(QWidget* parent = nullptr);
   ~SignerSettingsPage() override;

   void display() override;
   void reset() override;
   void apply() override;
   void initSettings() override;
   void init(const ApplicationSettings::State&) override;

   void onSignerSettings(const QList<SignerHost>&, const std::string& ownKey, int idxCur);

private slots:
   void onAsSpendLimitChanged(double);
   void onManageSignerKeys();

signals:
   void signersChanged();
   void setSigner(int);

private:
   void showHost(bool);
   void showZmqPubKey(bool);
   void showLimits(bool);
   void showSignerKeySettings(bool);

private:
   std::unique_ptr<Ui::SignerSettingsPage> ui_;
   SignersModel*     signersModel_{ nullptr };
   SignerKeysWidget* signerKeysWidget_{ nullptr };
   QList<SignerHost> signers_;
   int   curSignerIdx_{ 0 };
   std::string ownKey_;
};

#endif // __SIGNER_SETTINGS_PAGE_H__
