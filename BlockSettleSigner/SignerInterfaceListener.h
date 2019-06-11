#ifndef SIGNER_INTERFACE_LISTENER_H
#define SIGNER_INTERFACE_LISTENER_H

#include <memory>
#include <QObject>
#include "CoreWallet.h"
#include "SignContainer.h"
#include "DataConnectionListener.h"
#include "bs_signer.pb.h"

#include <QJSValueList>
#include <functional>

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
namespace spdlog {
   class logger;
}
class ZmqBIP15XDataConnection;
class SignerAdapter;

using namespace Blocksettle::Communication;


//template <typename... T>
//class Action {
//public:

//  using bind_type = decltype(std::bind(std::declval<std::function<void(T...)>>(),std::declval<T>()...));

//  template <typename... ConstrT>
//  Action(std::function<void(T...)> f, ConstrT&&... args)
//    : bind_(f,std::forward<ConstrT>(args)...)
//  { }

//  void act()
//  { bind_(); }

//private:
//  bind_type bind_;
//};

//int main()
//{
//  Action<int,int> add([](int x, int y)
//                      { std::cout << (x+y) << std::endl; },
//                      3, 4);

//  add.act();
//  return 0;
//}
namespace helper
{
    template <int... Is>
    struct index {};

    template <int N, int... Is>
    struct gen_seq : gen_seq<N - 1, N - 1, Is...> {};

    template <int... Is>
    struct gen_seq<0, Is...> : index<Is...> {};
}

template <typename... Ts>
class Action
{
private:
    std::function<void (Ts...)> f;
    std::tuple<Ts...> args;
public:
    template <typename F, typename... Args>
    Action(F&& func, Args&&... args)
        : f(std::forward<F>(func)),
          args(std::forward<Args>(args)...)
    {}

    template <typename... Args, int... Is>
    void func(std::tuple<Args...>& tup, helper::index<Is...>)
    {
        f(std::get<Is>(tup)...);
    }

    template <typename... Args>
    void func(std::tuple<Args...>& tup)
    {
        func(tup, helper::gen_seq<sizeof...(Args)>{});
    }

    void act()
    {
        func(args);
    }
};

class SignerInterfaceListener : public QObject, public DataConnectionListener
{
   Q_OBJECT

public:
   SignerInterfaceListener(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<ZmqBIP15XDataConnection> &conn, SignerAdapter *parent);

   void OnDataReceived(const std::string &) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   bs::signer::RequestId send(signer::PacketType pt, const std::string &data);
   std::shared_ptr<ZmqBIP15XDataConnection> getDataConnection() { return connection_; }

   void setTxSignCb(bs::signer::RequestId reqId, const std::function<void(const BinaryData &)> &cb) {
      cbSignReqs_[reqId] = cb;
   }
   void setWalletInfoCb(bs::signer::RequestId reqId
      , const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb) {
      cbWalletInfo_[reqId] = cb;
   }
   void setHDWalletDataCb(bs::signer::RequestId reqId, const std::function<void(bs::sync::HDWalletData)> &cb) {
      cbHDWalletData_[reqId] = cb;
   }
   void setWalletDataCb(bs::signer::RequestId reqId, const std::function<void(bs::sync::WalletData)> &cb) {
      cbWalletData_[reqId] = cb;
   }
   void setWatchOnlyCb(bs::signer::RequestId reqId, const std::function<void(const bs::sync::WatchingOnlyWallet &)> &cb) {
      cbWO_[reqId] = cb;
   }
   void setDecryptNodeCb(bs::signer::RequestId reqId
      , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &cb) {
      cbDecryptNode_[reqId] = cb;
   }
   void setReloadWalletsCb(bs::signer::RequestId reqId, const std::function<void()> &cb) {
      cbReloadWallets_[reqId] = cb;
   }
   void setChangePwCb(bs::signer::RequestId reqId, const std::function<void(bool)> &cb) {
      cbChangePwReqs_[reqId] = cb;
   }
   void setCreateHDWalletCb(bs::signer::RequestId reqId, const std::function<void(bool success, const std::string& errorMsg)> &cb) {
      cbCreateHDWalletReqs_[reqId] = cb;
   }
   void setDeleteHDWalletCb(bs::signer::RequestId reqId, const std::function<void(bool success, const std::string& errorMsg)> &cb) {
      cbDeleteHDWalletReqs_[reqId] = cb;
   }
   void setHeadlessPubKeyCb(bs::signer::RequestId reqId, const std::function<void(const std::string &pubKey)> &cb) {
      cbHeadlessPubKeyReqs_[reqId] = cb;
   }
   void setAutoSignCb(bs::signer::RequestId reqId, const std::function<void(bs::error::ErrorCode errorCode)> &cb) {
      cbAutoSignReqs_[reqId] = cb;
   }

   int reqId = 0;
   std::unordered_map<int, std::function<void(QJSValueList args)>> cbReqs;

//   template <typename T> T read(QJSValueList cbArgs, int index)
//   {
//      return static_cast<T>(cbArgs.at(index));
//   }

//   template <typename... Args>
//   std::tuple<Args...> parse(QJSValueList cbArgs)
//   {
//      return std::make_tuple(read<Args>(cbArgs )...);
//   }

   // Convert array into a tuple
   // https://en.cppreference.com/w/cpp/utility/integer_sequence
   template<typename ... T>
   auto a2t_impl(QJSValueList &args)
   {
      if (args.isEmpty()) {
         return std::make_tuple(T()...);
      }
      QJSValue arg = args.first();
      args.removeFirst();
      return std::make_tuple(dynamic_cast<T>(arg)...);
   }

   template<typename ... T>
   auto a2t(QJSValueList &args)
   {
       return a2t_impl(args);
   }

   template <typename ...CbType>
   void invokeQmlMethod(const char *method, std::function<void(CbType... Args)> cb,
                        QGenericArgument val0 = QGenericArgument(nullptr),
                        QGenericArgument val1 = QGenericArgument(),
                        QGenericArgument val2 = QGenericArgument(),
                        QGenericArgument val3 = QGenericArgument(),
                        QGenericArgument val4 = QGenericArgument(),
                        QGenericArgument val5 = QGenericArgument(),
                        QGenericArgument val6 = QGenericArgument(),
                        QGenericArgument val7 = QGenericArgument(),
                        QGenericArgument val8 = QGenericArgument())
   {
      reqId++;

      std::function<void(QJSValueList)> serializedCb = [this, cb](QJSValueList cbArgs){
         std::tuple<CbType ...> args = std::make_tuple<CbType ...>(cbArgs);

         //std::get<0>(args) = cbArgs.at(0);


         //std::tuple<CbType ...> args= a2t<CbType...>(cbArgs);

         cb(std::get<CbType>(args)...);
      };

      //cbReqs.insert(reqId, serializedCb);


//      QJSValue jsCallback;
//      QMetaObject::invokeMethod(rootObj_, "getJsCallback"
//                                , Q_ARG(QJSValue, qmlFactory->getJsCallback(reqId))
//                                , Q_RETURN_ARG(QJSValue, jsCallback)
//                                );

//      QMetaObject::invokeMethod(rootObj_, "invoke"
//                                , method, jsCallback
//                                , val0, val1, val2, val3, val4, val5, val6, val7
//                                );


   }

   Q_INVOKABLE void execJsCallback(int reqId, QJSValueList args)
   {
      const auto &itCb = cbReqs.find(reqId);
      if (itCb == cbReqs.end()) {
         logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
            , __func__, reqId);
         return;
      }

      itCb->second(args);
      cbReqs.erase(itCb);
   }

private:
   void processData(const std::string &);

   void onReady(const std::string &data);
   void onPeerConnected(const std::string &data, bool connected);
   void onSignTxRequested(const std::string &data);
   void onSignSettlementTxRequested(const std::string &data);
   void onTxSigned(const std::string &data, bs::signer::RequestId);
   void onCancelTx(const std::string &data, bs::signer::RequestId);
   void onXbtSpent(const std::string &data);
   void onAutoSignActivated(const std::string &data, bs::signer::RequestId reqId);
   void onSyncWalletInfo(const std::string &data, bs::signer::RequestId);
   void onSyncHDWallet(const std::string &data, bs::signer::RequestId);
   void onSyncWallet(const std::string &data, bs::signer::RequestId);
   void onCreateWO(const std::string &data, bs::signer::RequestId);
   void onDecryptedKey(const std::string &data, bs::signer::RequestId);
   void onReloadWallets(bs::signer::RequestId);
   void onExecCustomDialog(const std::string &data, bs::signer::RequestId);
   void onChangePassword(const std::string &data, bs::signer::RequestId);
   void onCreateHDWallet(const std::string &data, bs::signer::RequestId);
   void onDeleteHDWallet(const std::string &data, bs::signer::RequestId);
   void onHeadlessPubKey(const std::string &data, bs::signer::RequestId);
   void onUpdateStatus(const std::string &data);

   void shutdown();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<ZmqBIP15XDataConnection>  connection_;
   SignerAdapter  *  parent_;
   bs::signer::RequestId   seq_ = 1;
   std::map<bs::signer::RequestId, std::function<void(const BinaryData &)>>      cbSignReqs_;
   std::map<bs::signer::RequestId, std::function<void(std::vector<bs::sync::WalletInfo>)>>  cbWalletInfo_;
   std::map<bs::signer::RequestId, std::function<void(bs::sync::HDWalletData)>>  cbHDWalletData_;
   std::map<bs::signer::RequestId, std::function<void(bs::sync::WalletData)>>    cbWalletData_;
   std::map<bs::signer::RequestId, std::function<void(const bs::sync::WatchingOnlyWallet &)>>   cbWO_;
   std::map<bs::signer::RequestId
      , std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)>>   cbDecryptNode_;
   std::map<bs::signer::RequestId, std::function<void()>>   cbReloadWallets_;
   std::map<bs::signer::RequestId, std::function<void(bool success)>> cbChangePwReqs_;
   std::map<bs::signer::RequestId, std::function<void(bool success, const std::string& errorMsg)>> cbCreateHDWalletReqs_;
   std::map<bs::signer::RequestId, std::function<void(bool success, const std::string& errorMsg)>> cbDeleteHDWalletReqs_;
   std::map<bs::signer::RequestId, std::function<void(const std::string &pubKey)>> cbHeadlessPubKeyReqs_;
   std::map<bs::signer::RequestId, std::function<void(bs::error::ErrorCode errorCode)>> cbAutoSignReqs_;
};


#endif // SIGNER_INTERFACE_LISTENER_H
