#ifndef SANDBOX_PRINT_H_
#define SANDBOX_PRINT_H_

#include "Library/DebugLib.h"

#define SANDBOX_DEBUG 1

#if SANDBOX_DEBUG

#define SBDebug(format, ...)                                                   \
  DebugPrint(DEBUG_INFO, "[Sandbox] (%a:%d) " format, __func__, __LINE__, ##__VA_ARGS__)

#else

#define SBDebug(format, ...)

#endif

#define SBPrint(format, ...)                                                   \
  DebugPrint(DEBUG_INFO, "[Sandbox] (%a:%d) " format, __func__, __LINE__, ##__VA_ARGS__)
#define SBWarn(format, ...)                                                    \
  DebugPrint(DEBUG_WARN, "[Sandbox] (%a:%d) " format, __func__, __LINE__, ##__VA_ARGS__)
#define SBError(format, ...)                                                   \
  DebugPrint(DEBUG_ERROR, "[Sandbox] (%a:%d) " format, __func__, __LINE__, ##__VA_ARGS__)

#define __unimplemented(format, ...)                                           \
  do {                                                                         \
    SBError("Unimplemented Behavior" format "(%a:%d)\n", ##__VA_ARGS__,        \
            __FILE__, __LINE__);                                               \
    while (1)                                                                  \
      ;                                                                        \
  } while (0)

#define UNUSED(x) (void)(x)

#define __unreachable(format, ...)                                             \
  do {                                                                         \
    SBError("Unreachable Code: " format "(%a:%d)\n", ##__VA_ARGS__, __FILE__,    \
            __LINE__);                                                         \
    while (1)                                                                  \
      ;                                                                        \
  } while (0)

#endif
