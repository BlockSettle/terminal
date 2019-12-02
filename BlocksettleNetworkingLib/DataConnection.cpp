/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "DataConnection.h"

#include <cassert>

void DataConnection::detachFromListener()
{
   listener_ = nullptr;
}

void DataConnection::notifyOnData(const std::string& data)
{
   if (listener_) {
      listener_->OnDataReceived(data);
   }
}

void DataConnection::notifyOnConnected()
{
   if (listener_) {
      listener_->OnConnected();
   }
}

void DataConnection::notifyOnDisconnected()
{
   if (listener_) {
      listener_->OnDisconnected();
   }
}

void DataConnection::notifyOnError(DataConnectionListener::DataConnectionError errorCode)
{
   if (listener_) {
      listener_->OnError(errorCode);
   }
}