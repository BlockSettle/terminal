/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CUSTOMCOMBOBOX_H
#define CUSTOMCOMBOBOX_H

#include <QComboBox>

QT_BEGIN_NAMESPACE
class QListView;
QT_END_NAMESPACE

class CustomComboBox : public QComboBox
{
   Q_OBJECT
   Q_PROPERTY(bool firstItemHidden
              READ isFirstItemHidden
              WRITE setFirstItemHidden
              NOTIFY firstItemHiddenChanged)

public:
   explicit CustomComboBox(QWidget *parent = nullptr);
   ~CustomComboBox() override;

   void showPopup() override;
   void hidePopup() override;

   bool isFirstItemHidden() const;

public slots:
   void setFirstItemHidden(bool firstItemHidden);

private slots:
   void showFirstItem();
   void hideFirstItem();

signals:
   void showPopupTriggered();
   void hidePopupTriggered();
   void firstItemHiddenChanged(bool firstItemHidden);

private:
   QScopedPointer<QListView, QScopedPointerDeleteLater> listView_;
   bool firstItemHidden_;
};

#endif // CUSTOMCOMBOBOX_H
