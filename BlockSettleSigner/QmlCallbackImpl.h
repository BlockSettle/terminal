#ifndef QML_CALLBACK_IMPL_H
#define QML_CALLBACK_IMPL_H

#include <QObject>
#include <QQmlEngine>
#include <QJSValueList>

#include <functional>
#include <tuple>
#include <memory>
#include <type_traits>

namespace bs {
namespace signer {

namespace cbhelper
{
    template <int... Is>
    struct index {};

    template <int N, int... Is>
    struct gen_seq : gen_seq<N - 1, N - 1, Is...> {};

    template <int... Is>
    struct gen_seq<0, Is...> : index<Is...> {};
} // namespace cbhelper


class QmlCallbackBase : public QObject
{
   Q_OBJECT
public:
   QmlCallbackBase (QObject *parent = nullptr)
      : QObject(parent){}
   Q_INVOKABLE void exec(const QVariantList &jsValues){
      setJsValues(jsValues);
      execImpl();
   }
   virtual void execImpl() = 0;
   virtual void setJsValues(const QVariantList &jsValues) = 0;
};

template <typename... Ts>
class QmlCallback : public QmlCallbackBase
{
private:
    std::function<void (Ts...)> f;
    std::tuple<Ts...> args_;
public:
   template <typename F, typename... Args>
   QmlCallback(F&& func, Args&&... args)
      : f(std::forward<F>(func)),
        args_(std::forward<Args>(args)...)
   {}

   template <typename... Args, int... Is>
   void func(std::tuple<Args...>& tup, cbhelper::index<Is...>)
   {
      f(std::get<Is>(tup)...);
   }

   template <typename... Args>
   void func(std::tuple<Args...>& tup)
   {
      func(tup, cbhelper::gen_seq<sizeof...(Args)>{});
   }

   void execImpl() override
   {
      func(args_);
   }

   void setJsValues(const QVariantList &jsValues) override
   {
      if (sizeof...(Ts) > 0) {
         setJsValuesGen<sizeof...(Ts)-1>(jsValues);
      }
   }


private:
   template <typename... Args>
   void setArgs(Args&&... args)
   {
      args_ = std::make_tuple(std::forward<Args>(args)...);
   }

   // iterate tuple from end to begin and set values from QJSValueList
//   template <int N>
//   void setJsValuesImpl(const QJSValueList &jsValues)
//   {
//      if (N > 0) {
//         setJsValuesGen<N-1>(jsValues);
//      }
//   }

   template <int N>
   std::enable_if_t<N == 0> setJsValuesGen(const QVariantList &jsValues) {
      setJsValue<N>(jsValues);
   }

   template <int N>
   std::enable_if_t<N != 0> setJsValuesGen(const QVariantList &jsValues) {
      setJsValue<N>(jsValues);
      setJsValuesGen<N-1>(jsValues);
   }

   template <int Is>
   void setJsValue(const QVariantList &jsValues)
   {
      using tuple_element_t = typename std::tuple_element<Is, decltype(args_)>::type;
      if (jsValues.size() > Is) {
         std::get<Is>(args_) = qvariant_cast<tuple_element_t>(jsValues.at(Is));
      }
   }
};

} // namespace signer
} // namespace bs




#endif // QML_CALLBACK_IMPL_H
