/**
 * Copyright (c) 2024, sulfurandcu.github.io
 *
 * SPDX-License-Identifier: MIT
 *
 * Change Logs:
 * Date             Author              Notes
 * 2024-10-24       liujitong           first version
 */

#include "hpatch_impl.h"
#include "hpatch_conf.h"

hpi_patch_result_t hpi_patch(hpatchi_listener_t *listener, int patch_cache_size, int decompress_cache_size, hpi_TInputStream_read _do_read_diff, read_old_t _do_read_old, write_new_t _do_write_new)
{
    hpi_patch_result_t result = HPATCHI_SUCCESS;
    hpi_byte *pmem            = 0;
    hpi_byte *patch_cache;

    hpi_compressType compress_type;
    hpi_pos_t new_size;
    hpi_pos_t uncompress_size;

    hpi_BOOL patch_result;
    patch_result = hpatch_lite_open(listener, _do_read_diff, &compress_type, &new_size, &uncompress_size);
    if (patch_result != hpi_TRUE)
    {
        result = HPATCHI_PATCH_OPEN_ERROR;
        goto clear;
    }

    listener->read_old  = _do_read_old;
    listener->write_new = _do_write_new;

    switch (compress_type)
    {
        case hpi_compressType_no: // memory size: patch_cache_size
        {
            pmem = (hpi_byte *)hpi_malloc(patch_cache_size);
            if (!pmem)
            {
                result = HPATCHI_MEM_ERROR;
                goto clear;
            }
            patch_cache = pmem;

            listener->diff_data = listener;
            listener->read_diff = _do_read_diff;
        }
        break;
#ifdef _CompressPlugin_tuz
        case hpi_compressType_tuz: // requirements memory size: patch_cache_size + decompress_cache_size + decompress_dict_size
        {
            tuz_TStream tuz_stream_handle;

            size_t decompress_dict_size = _tuz_TStream_getReservedMemSize(listener, _do_read_diff);
            if (decompress_dict_size <= 0)
            {
                result = HPATCHI_DECOMPRESSER_DICT_ERROR;
                goto clear;
            }

            pmem = (hpi_byte *)hpi_malloc(decompress_dict_size + decompress_cache_size + patch_cache_size);
            if (!pmem)
            {
                result = HPATCHI_MEM_ERROR;
                goto clear;
            }

            tuz_TResult tuz_result = tuz_TStream_open(&tuz_stream_handle, listener, _do_read_diff, pmem, (tuz_size_t)decompress_dict_size, (tuz_size_t)decompress_cache_size);
            if (tuz_result != tuz_OK)
            {
                result = HPATCHI_DECOMPRESSER_OPEN_ERROR;
                goto clear;
            }

            patch_cache = pmem + decompress_dict_size + decompress_cache_size;

            listener->diff_data = &tuz_stream_handle;
            listener->read_diff = _tuz_TStream_decompress;
        }
        break;
#endif
        default:
        {
            result = HPATCHI_COMPRESSTYPE_ERROR;
            goto clear;
        }
    }

    patch_result = hpatch_lite_patch(listener, new_size, patch_cache, (hpi_size_t)patch_cache_size);
    if (patch_result != hpi_TRUE)
    {
        result = HPATCHI_PATCH_ERROR;
        goto clear;
    }

clear:
    if (pmem)
    {
        hpi_free(pmem);
        pmem = 0;
    }
    return result;
}
