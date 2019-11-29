/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
/*
 * Enable all warnings.
 * Usage:
 * #include <disable_warnings.h>
 * #include <header_with_uninteresting_warnings.h
 * #include <enable_warnings.h>
 */

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if defined(__GNUC__)
  #if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)
    #pragma GCC diagnostic pop
  #endif
#endif // __GNUC__

