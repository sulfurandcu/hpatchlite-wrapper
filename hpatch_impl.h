/**
 * Copyright (c) 2024, sulfurandcu.github.io
 *
 * SPDX-License-Identifier: MIT
 *
 * Change Logs:
 * Date             Author              Notes
 * 2024-10-24       liujitong           first version
 */

#ifndef __hpatch_impl_h__
#define __hpatch_impl_h__

#include "hpatch_lite.h"
#include "patch_types.h"
#include "decompresser_demo.h"

typedef enum hpi_patch_result_t
{
    HPATCHI_SUCCESS = 0,
    HPATCHI_OPTIONS_ERROR,
    HPATCHI_PATHTYPE_ERROR,
    HPATCHI_OPENREAD_ERROR,
    HPATCHI_OPENWRITE_ERROR,
    HPATCHI_FILEREAD_ERROR,
    HPATCHI_FILEWRITE_ERROR,
    HPATCHI_FILEDATA_ERROR,
    HPATCHI_FILECLOSE_ERROR,
    HPATCHI_MEM_ERROR,
    HPATCHI_COMPRESSTYPE_ERROR,
    HPATCHI_DECOMPRESSER_DICT_ERROR,
    HPATCHI_DECOMPRESSER_OPEN_ERROR,
    HPATCHI_DECOMPRESSER_CLOSE_ERROR,
    HPATCHI_PATCH_OPEN_ERROR = 20,
    HPATCHI_PATCH_ERROR,
} hpi_patch_result_t;

typedef hpi_BOOL (*read_old_t)(struct hpatchi_listener_t *listener, hpi_pos_t addr, hpi_byte *data, hpi_size_t size);
typedef hpi_BOOL (*write_new_t)(struct hpatchi_listener_t *listener, const hpi_byte *data, hpi_size_t size);

hpi_patch_result_t hpi_patch(hpatchi_listener_t *listener, int patch_cache_size, int decompress_cache_size,
                             hpi_TInputStream_read _do_read_diff, read_old_t _do_read_old, write_new_t _do_write_new);

#endif /* __hpatch_impl_h__ */
