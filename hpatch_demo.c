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

typedef struct hpatchi_instance_t
{
    hpatchi_listener_t parent;
    int patch_file_offset;
    int patch_file_len;
    int patch_read_pos;
    int newer_file_len;
    int newer_write_pos;
} hpatchi_instance_t;

// 以数据流的形式读取空文件
hpi_BOOL _do_read_empty(struct hpatchi_listener_t *listener, hpi_pos_t addr, hpi_byte *data, hpi_size_t size)
{
    memset(data, 0, size);
    return hpi_TRUE;
}

// 以数据流的形式读取旧程序
hpi_BOOL _do_read_old(struct hpatchi_listener_t *listener, hpi_pos_t addr, hpi_byte *data, hpi_size_t size)
{
    int result = update_fetch_runapp(addr, data, size);
    if (result < 0) { return hpi_FALSE; }
    return hpi_TRUE;
}

// 以数据流的形式读取补丁包（由用户记录数据流的位置：读到哪儿了）
hpi_BOOL _do_read_patch(hpi_TInputStreamHandle input_stream, hpi_byte *data, hpi_size_t *size)
{
    hpatchi_instance_t *instance = (hpatchi_instance_t *)input_stream;

    // 数据流结束时需要将 *size 置为当前所读数据的实际长度（*size == decompress_cache_size）
    if ((instance->patch_read_pos + *size) > instance->patch_file_len)
    {
        *size = instance->patch_file_len - instance->patch_read_pos;
    }

    int result = update_fetch_backup(instance->patch_file_offset + instance->patch_read_pos, data, *size);
    if (result < 0) { return hpi_FALSE; }
    instance->patch_read_pos += *size;
    return hpi_TRUE;
}

// 以数据流的形式写入差分还原的数据（由用户记录数据流的位置：写到哪儿了）
hpi_BOOL _do_write_new(struct hpatchi_listener_t *listener, const hpi_byte *data, hpi_size_t size)
{
    hpatchi_instance_t *instance = (hpatchi_instance_t *)listener;

    int percent = instance->newer_write_pos * 100 / instance->newer_file_len;
    if (percent % 5 == 0 && percent < 100)
    {
        rt_kprintf("\b\b\b%02d%%", percent);
    }

    int result = update_write_decode(instance->newer_write_pos, (unsigned char *)data, size);
    if (result < 0) { return hpi_FALSE; }
    instance->newer_write_pos += size;
    return hpi_TRUE;
}

void demo(void)
{
    hpatchi_instance_t instance = {0};
    instance.patch_file_offset  = update_pack->header_size; // 从升级包的包头中获取差分数据的地址
    instance.patch_file_len     = update_pack->remain_size; // 从升级包的包头中获取差分数据的大小
    instance.newer_file_len     = update_pack->newapp_size; // 从升级包的包头中获取新版程序的大小

    // 差分全量升级
    {
        hpi_patch(&instance.parent, 128, 128, _do_read_patch, _do_read_empty, _do_write_new);
    }

    // 差分增量升级
    {
        hpi_patch(&instance.parent, 128, 128, _do_read_patch, _do_read_old, _do_write_new);
    }
}
