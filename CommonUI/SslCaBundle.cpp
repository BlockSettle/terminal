/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "SslCaBundle.h"

#include <mutex>
#include <QFile>

namespace {

   std::once_flag g_initialized;

   std::string g_caBundle;

   void init()
   {
      std::call_once(g_initialized, [] {
         QFile f(QStringLiteral(":/resources/ca-bundle.crt"));
         if (!f.open(QFile::ReadOnly)) {
            throw std::runtime_error("can't open ':/resources/ca-bundle.crt' resource");
         }
         auto content = f.readAll();
         if (content.isEmpty()) {
            throw std::runtime_error("empty CA bundle");
         }

         g_caBundle = content.toStdString();
      });
   }

}

const void *bs::caBundlePtr()
{
   init();
   return g_caBundle.c_str();
}

unsigned bs::caBundleSize()
{
   init();
   return static_cast<unsigned>(g_caBundle.size());
}
