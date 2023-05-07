/*
***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************
*/
#pragma once

#include <QObject>

class ScaleController : public QObject
{
   Q_OBJECT
   Q_PROPERTY(qreal scaleRatio READ scaleRatio NOTIFY changed)
   Q_PROPERTY(int screenWidth READ screenWidth NOTIFY changed)
   Q_PROPERTY(int screenHeight READ screenHeight NOTIFY changed)
public:
   ScaleController(QObject* parent = nullptr);

   qreal scaleRatio() const { return scaleRatio_; }
   int screenWidth() const { return screenWidth_; }
   int screenHeight() const { return screenHeight_; }

   Q_INVOKABLE void update();

signals:
   void changed();

private:
   int screenWidth_;
   int screenHeight_;
   qreal scaleRatio_{ 1.0 };
};
