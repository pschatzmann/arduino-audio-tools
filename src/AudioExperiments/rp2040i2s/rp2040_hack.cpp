#include "i2s_config.h"

#if ADD_RP2040_SYNC
extern "C" void __sync_synchronize(){}
#endif