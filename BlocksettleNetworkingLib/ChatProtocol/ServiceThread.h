/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SERVICETHREAD_H
#define SERVICETHREAD_H

#include <QThread>

namespace Chat
{

   template <typename TServiceWorker>
   class ServiceThread : public QThread
   {
   public:
      explicit ServiceThread(TServiceWorker* worker, QObject* parent = nullptr)
         : QThread(parent), worker_(worker)
      {
         worker_->moveToThread(this);
         start();
      }

      ~ServiceThread()
      {
         quit();
         wait();
      }

      TServiceWorker* worker() const
      {
         return worker_;
      }

   protected:
      void run() override
      {
         QThread::run();
         delete worker_;
      }

   private:
      TServiceWorker* worker_;
   };

}

#endif // SERVICETHREAD_H
