#ifndef LOGGER_HELPERS_H
#define LOGGER_HELPERS_H

#include <spdlog/spdlog.h>

// checks boolean result and returns value if check failed
#define BS_ASSERT_RETURN_VALUE(logger, result, value) \
   if (!(result)) { \
      SPDLOG_LOGGER_CRITICAL(logger, "assert failed: {}", #result); \
      return value; \
   }

// checks boolean result and returns if check failed
#define BS_ASSERT_RETURN(logger, result) BS_ASSERT_RETURN_VALUE(logger, result, )

// checks boolean result and returns false if check failed
#define BS_ASSERT_RETURN_FALSE(logger, result) BS_ASSERT_RETURN_VALUE(logger, result, false)



// checks boolean result and returns value if check failed
#define BS_VERIFY_RETURN_VALUE(logger, result, value) \
   if (!(result)) { \
      SPDLOG_LOGGER_ERROR(logger, "check failed: {}", #result); \
      return value; \
   }

// checks boolean result and returns if check failed
#define BS_VERIFY_RETURN(logger, result) BS_VERIFY_RETURN_VALUE(logger, result, )

// checks boolean result and returns false if check failed
#define BS_VERIFY_RETURN_FALSE(logger, result) BS_VERIFY_RETURN_VALUE(logger, result, false)




#endif
