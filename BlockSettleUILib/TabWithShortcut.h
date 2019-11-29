/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#ifndef _TABWITHSHORTCUT_H_INCLUDED_
#define _TABWITHSHORTCUT_H_INCLUDED_

#include <QWidget>


//
// TabWithShortcut
//

//! Base class for widget with shortcuts.
class TabWithShortcut : public QWidget {
   Q_OBJECT

public:
   //! Shortcut type.
   enum class ShortcutType {
      Alt_1,
      Alt_2,
      Alt_3,
      Ctrl_S,
      Ctrl_P,
      Ctrl_Q,
      Alt_S,
      Alt_P,
      Alt_B
   }; // enum ShortcutType

   explicit TabWithShortcut(QWidget *parent);
   ~TabWithShortcut() noexcept override = default;

   virtual void shortcutActivated(ShortcutType s) = 0;
}; // class TabWithShortcut

#endif // _TABWITHSHORTCUT_H_INCLUDED_
