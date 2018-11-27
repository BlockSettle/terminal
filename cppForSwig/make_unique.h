#ifndef _H_MAKE_UNIQUE
#define _H_MAKE_UNIQUE

// Pre-C++14 compilers don't have access to make_unique. Use a workaround on
// those compilers. Once VS2017 is a required compiler, we can start to rely
// solely on __cplusplus, but for now, we must use _MSC_VER on Windows.
#ifndef _WIN32
#include <memory>
#if __cplusplus >= 201402L
#define make_unique std::make_unique
#else
template<typename T, typename... Args> std::unique_ptr<T> make_unique(Args&&... args)
{
   return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif
#else
#if _MSC_VER < 1900
template<typename T, typename... Args> std::unique_ptr<T> make_unique(Args&&... args)
{
   return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#else
#define make_unique std::make_unique
#endif
#endif
#endif
