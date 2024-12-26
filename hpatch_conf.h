/**
 * Copyright (c) 2024, sulfurandcu.github.io
 *
 * SPDX-License-Identifier: MIT
 *
 * Change Logs:
 * Date             Author              Notes
 * 2024-10-24       liujitong           first version
 */

#ifndef __hpatch_conf_h__
#define __hpatch_conf_h__

#include "rtthread.h"

#define hpi_malloc(x) rt_malloc(x)
#define hpi_free(x)   rt_free(x)

#endif /* __hpatch_conf_h__ */
