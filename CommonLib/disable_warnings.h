/*
 * Disable specified warnings.
 * Usage:
 * #include <disable_warnings.h>
 * #include <header_with_uninteresting_warnings.h
 * #include <enable_warnings.h>
 */

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4996)
#pragma warning(disable : 4250)
#pragma warning(disable : 4251)
#pragma warning(disable : 4275)
#endif

#if defined(__GNUC__)
  #if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
  #endif
#endif // __GNUC__

