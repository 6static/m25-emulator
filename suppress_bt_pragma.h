/*
 * suppress_bt_pragma.h - forced-included via build_flags: -include suppress_bt_pragma.h
 *
 * Problem:  The pre-built framework ships sdkconfig.h with
 *             CONFIG_BT_HFP_ENABLE = 1
 *             CONFIG_BTDM_CTRL_BR_EDR_MAX_SYNC_CONN = 0
 *           Both conditions trigger a #pragma message in esp_bredr_cfg.h:
 *             "BT: forcing BR/EDR max sync conn eff to 1 (Bluedroid HFP requires SCO/eSCO)"
 *           sdkconfig.defaults cannot override pre-built headers (they are already
 *           compiled into the framework package).
 *
 * Fix:      Force sdkconfig.h to be consumed here first (#pragma once prevents a
 *           second inclusion), then override CONFIG_BT_HFP_ENABLE=0.
 *           HFP is not used by this firmware, only SPP/BLE is needed.
 */
#pragma once
#include "sdkconfig.h"

/* HFP (Hands-Free Profile) is unused; disabling it makes the pragma condition false. */
#ifdef CONFIG_BT_HFP_ENABLE
#undef  CONFIG_BT_HFP_ENABLE
#endif
#define CONFIG_BT_HFP_ENABLE 0

#ifdef CONFIG_HFP_ENABLE
#undef  CONFIG_HFP_ENABLE
#endif
#define CONFIG_HFP_ENABLE 0
