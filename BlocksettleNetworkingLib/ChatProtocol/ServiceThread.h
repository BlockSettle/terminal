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
