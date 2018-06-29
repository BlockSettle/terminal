
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
      Alt_S,
      Alt_P,
      Alt_Q
   }; // enum ShortcutType

   explicit TabWithShortcut(QWidget *parent);
   ~TabWithShortcut() noexcept override = default;

   virtual void shortcutActivated(ShortcutType s) = 0;
}; // class TabWithShortcut

#endif // _TABWITHSHORTCUT_H_INCLUDED_
