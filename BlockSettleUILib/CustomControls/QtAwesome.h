/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
/**
 * QtAwesome - use font-awesome (or other font icons) in your c++ / Qt Application
 *
 * MIT Licensed
 *
 * Copyright 2013-2015 - Reliable Bits Software by Blommers IT. All Rights Reserved.
 * Author Rick Blommers
 */

#ifndef QTAWESOME_H
#define QTAWESOME_H

#include <QIcon>
#include <QIconEngine>
#include <QPainter>
#include <QRect>
#include <QVariantMap>


/// A list of all icon-names with the codepoint (unicode-value) on the right
/// You can use the names on the page  http://fortawesome.github.io/Font-Awesome/design.html
namespace infinity {
  enum icon {
     down               = 0xe91c,
     user_2             = 0xe600,
  };
}


//---------------------------------------------------------------------------------------

class QtAwesomeIconPainter;

/// The main class for managing icons
/// This class requires a 2-phase construction. You must first create the class and then initialize it via an init* method
class QtAwesome : public QObject
{
Q_OBJECT

public:

    explicit QtAwesome(QObject *parent = nullptr);
    ~QtAwesome() noexcept override;

    void init( const QString& fontname );
    bool initInfinity();

    void addNamedCodepoint( const QString& name, int codePoint );
    QHash<QString,int> namedCodePoints() { return namedCodepoints_; }

    void setDefaultOption( const QString& name, const QVariant& value  );
    void setDefaultOption( const char* name, const QVariant& value  ) {
       setDefaultOption(QLatin1String(name), value);
   }
    QVariant defaultOption( const QString& name );

    QIcon icon( int character, const QVariantMap& options = QVariantMap() );
    QIcon icon( const QString& name, const QVariantMap& options = QVariantMap() );
    QIcon icon(QtAwesomeIconPainter* painter, const QVariantMap& optionMap = QVariantMap() );

    void give( const QString& name, QtAwesomeIconPainter* painter );

    QFont font( int size );

    /// Returns the font-name that is used as icon-map
    QString fontName() { return fontName_ ; }

private:
    QString fontName_;                                     ///< The font name used for this map
    QHash<QString,int> namedCodepoints_;                   ///< A map with names mapped to code-points

    QHash<QString, QtAwesomeIconPainter*> painterMap_;     ///< A map of custom painters
    QVariantMap defaultOptions_;                           ///< The default icon options
    QtAwesomeIconPainter* fontIconPainter_;                ///< A special painter fo painting codepoints
};


//---------------------------------------------------------------------------------------


/// The QtAwesomeIconPainter is a specialized painter for painting icons
/// your can implement an iconpainter to create custom font-icon code
class QtAwesomeIconPainter
{
public:
    virtual ~QtAwesomeIconPainter() {}
    virtual void paint( QtAwesome* awesome, QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state, const QVariantMap& options ) = 0;
};



#endif // QTAWESOME_H
