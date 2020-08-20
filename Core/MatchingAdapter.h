/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef MATCHING_ADAPTER_H
#define MATCHING_ADAPTER_H

#include <QObject>
#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}
class CelerClientProxy;

class MatchingAdapter : public QObject, public bs::message::Adapter
{
   Q_OBJECT
public:
   MatchingAdapter(const std::shared_ptr<spdlog::logger> &);
   ~MatchingAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "Matching"; }

private slots:
   void onCelerConnected();
   void onCelerDisconnected();
   void onCelerConnectionError(int);

private:

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_;
   std::shared_ptr<CelerClientProxy>   celerConnection_;
};


#endif	// MATCHING_ADAPTER_H
