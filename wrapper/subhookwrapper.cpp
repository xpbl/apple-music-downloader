#include <cstdint>
#include "subhook/subhook.h"
extern "C" void _ZN13mediaplatform26DebugLogEnabledForPriorityENS_11LogPriorityE();

extern "C" void initialize_subhook()
{
    subhook_t hook = subhook_new(
        (void *)_ZN13mediaplatform26DebugLogEnabledForPriorityENS_11LogPriorityE,
        (void *)1);
    subhook_install(hook);
}
