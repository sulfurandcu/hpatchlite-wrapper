/**
 * Copyright (c) 2024, sulfurandcu.github.io
 *
 * SPDX-License-Identifier: MIT
 *
 * Change Logs:
 * Date             Author              Notes
 * 2024-10-24       liujitong           first version
 */
#if 0
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
    int result = fal_read_old(addr, data, size);
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

    int result = fal_read_patch(instance->patch_file_offset + instance->patch_read_pos, data, *size);
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

    int result = fal_write_new(instance->newer_write_pos, (unsigned char *)data, size);
    if (result < 0) { return hpi_FALSE; }
    instance->newer_write_pos += size;
    return hpi_TRUE;
}

void demo(void)
{
    hpatchi_instance_t instance = {0};
    instance.patch_file_offset  = ; // 从升级包的包头中获取差分数据的地址
    instance.patch_file_len     = ; // 从升级包的包头中获取差分数据的大小
    instance.newer_file_len     = ; // 从升级包的包头中获取新版程序的大小

    // 差分全量升级
    {
        hpi_patch(&instance.parent, 128, 128, _do_read_patch, _do_read_empty, _do_write_new);
    }

    // 差分增量升级
    {
        hpi_patch(&instance.parent, 128, 128, _do_read_patch, _do_read_old, _do_write_new);
    }
}
#endif

/**
 * @file qboot_hpatchlite.c
 * @brief In-place OTA update implementation using HPatchLite.
 * @author huangly ()
 * @version 1.4
 * @date 2025-09-29
 * 
 * @copyright Copyright (c) 2025  
 * 
 * @note Supports both FLASH swap and RAM buffer strategies via Kconfig.
 *       This implementation is designed for in-place updates where the old firmware
 *       partition is directly overwritten to become the new firmware partition.
 * 
 * @par Modification log:
 * Date       Version Author      Description
 * 2025-09-25 1.0     huangly     First version with flash swap.
 * 2025-09-26 1.1     huangly     Refactored to support both RAM and FLASH buffer strategies.
 * 2025-09-27 1.2     huangly     Refined with Kconfig integration, ULOG, Doxygen, and bug fixes.
 * 2025-09-29 1.4     huangly     Completed RAM buffer strategy based on user's unified logic.
 */

#include "qboot_hpatchlite.h"

#include <rtthread.h>

// Only compile this file if HPatchLite is enabled
#ifdef QBOOT_USING_HPATCHLITE

#include "hpatch_impl.h"

// Define ULOG tag and level
#define DBG_TAG "qboot.hpatch"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/**
 * @brief Instance structure to hold state during the patching process.
 *        Conditionally includes members based on the selected buffer strategy.
 */
typedef struct hpatchi_instance_t
{
    hpatchi_listener_t parent;              /**< HPatchLite listener callbacks */
    int patch_file_offset;                  /**< Starting offset of the patch data within the patch partition */
    int patch_file_len;                     /**< Total length of the patch data */
    int patch_read_pos;                     /**< Current read position within the patch data stream */
    int newer_file_len;                     /**< Expected final length of the new firmware */
    int newer_write_pos;                    /**< Logical current write position in the new firmware */
    int progress_percent;                   /**< Last reported progress percentage to avoid duplicate logs */

    const fal_partition_t patch_part;       /**< FAL partition handle for the patch data */
    const fal_partition_t old_part;         /**< FAL partition handle for the old firmware (the one being updated) */

#if defined(QBOOT_HPATCH_USE_FLASH_SWAP)
    // --- Members for FLASH swap strategy ---
    fal_partition_t swap_part;              /**< FAL partition handle for the swap buffer */
    int swap_offset;                        /**< Starting offset of the usable area within the swap partition */
    int swap_write_pos;                     /**< Current write position within the swap buffer area */
    int swap_size;                          /**< Total usable size of the swap buffer area */
#elif defined(QBOOT_HPATCH_USE_RAM_BUFFER)
    // --- Members for RAM buffer strategy ---
    uint8_t *swap_buffer;                   /**< Pointer to the RAM buffer */
    int swap_buffer_size;                   /**< Size of the RAM buffer */
    int swap_write_pos;                     /**< Current write position within the RAM buffer */
#endif
    int committed_len;                      /**< Total length of data already committed from RAM/Flash to the old partition */

} hpatchi_instance_t;


// -----------------------------------------------------------------------------
// Common Listener Callbacks
// -----------------------------------------------------------------------------
/**
 * @brief HPatchLite listener: Reads patch data as a stream.
 * @param input_stream  Handle to the instance, used to track stream position.
 * @param data          Pointer to the buffer to fill with patch data.
 * @param size          Pointer to the requested data size (input) and actual read size (output).
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL _do_read_patch(hpi_TInputStreamHandle input_stream, hpi_byte *data, hpi_size_t *size)
{
    hpatchi_instance_t *instance = (hpatchi_instance_t *)input_stream;

    //If it has already been read or out of bounds, return 0 bytes directly.
    if (instance->patch_read_pos >= instance->patch_file_len)
    {
        *size = 0;
        return hpi_TRUE;
    }

    // Calculate the remaining length and adjust the length of this read
    hpi_size_t remaining_len = instance->patch_file_len - instance->patch_read_pos;
    if (*size > remaining_len)
    {
        *size = remaining_len;
    }

    //If *size is adjusted to 0, it indicates that reading is complete.
    if (*size == 0)
    {
        return hpi_TRUE;
    }

    if (fal_partition_read(instance->patch_part, instance->patch_file_offset + instance->patch_read_pos, data, *size) < 0)
    {
        LOG_E("Failed to read patch data from partition '%s'.", instance->patch_part->name);
        *size = 0;
        return hpi_FALSE;
    }
    instance->patch_read_pos += *size;
    return hpi_TRUE;
}

/**
 * @brief HPatchLite listener: Reads old data directly from the old partition.
 *        This function's behavior is IDENTICAL for both buffer strategies.
 * @param listener  Pointer to the listener instance.
 * @param addr      Absolute address within the old firmware to read from.
 * @param data      Pointer to the buffer to fill with old data.
 * @param size      Number of bytes to read.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL _do_read_old(struct hpatchi_listener_t *listener, hpi_pos_t addr, hpi_byte *data, hpi_size_t size)
{
    hpatchi_instance_t *instance = (hpatchi_instance_t *)listener;
    return (fal_partition_read(instance->old_part, addr, data, size) >= 0);
}


// -----------------------------------------------------------------------------
// FLASH Swap Strategy Implementation
// -----------------------------------------------------------------------------
#if defined(QBOOT_HPATCH_USE_FLASH_SWAP)

/**
 * @brief Copies data from one FAL partition to another using a temporary RAM buffer.
 * @param src_part      Source partition handle.
 * @param src_offset    Starting offset in the source partition.
 * @param dst_part      Destination partition handle.
 * @param dst_offset    Starting offset in the destination partition.
 * @param size          Total number of bytes to copy.
 * @return 0 on success, non-zero on failure.
 */
static int flash_to_flash_copy(const fal_partition_t src_part, int src_offset,
                               const fal_partition_t dst_part, int dst_offset, int size)
{
    uint8_t *buffer = RT_NULL;
    int result = 0;
    int remaining_len = size;
    int current_src_offset = src_offset;
    int current_dst_offset = dst_offset;

    buffer = rt_malloc(QBOOT_HPATCH_COPY_BUFFER_SIZE);
    if (buffer == RT_NULL)
    {
        LOG_E("Failed to malloc %d bytes for flash copy buffer!", QBOOT_HPATCH_COPY_BUFFER_SIZE);
        return -1;
    }

    LOG_D("Starting flash-to-flash copy of %d bytes...", size);

    while (remaining_len > 0)
    {
        int chunk_size = (remaining_len > QBOOT_HPATCH_COPY_BUFFER_SIZE) ? QBOOT_HPATCH_COPY_BUFFER_SIZE : remaining_len;

        if (fal_partition_read(src_part, current_src_offset, buffer, chunk_size) < 0)
        {
            LOG_E("Flash copy failed at read step from '%s'!", src_part->name);
            result = -1;
            break;
        }

        if (fal_partition_write(dst_part, current_dst_offset, buffer, chunk_size) < 0)
        {
            LOG_E("Flash copy failed at write step to '%s'!", dst_part->name);
            result = -2;
            break;
        }

        current_src_offset += chunk_size;
        current_dst_offset += chunk_size;
        remaining_len -= chunk_size;
    }

    rt_free(buffer);

    if (result == 0)
    {
        LOG_D("Flash copy successful.");
    }
    return result;
}

/**
 * @brief Commits the buffered data from the swap partition to the old partition.
 *        This involves erasing the target area on the old partition, copying data,
 *        and then erasing the swap partition for the next round.
 * @param instance Pointer to the patch instance.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL _commit_swap_to_old_flash(hpatchi_instance_t *instance)
{
    if (instance->swap_write_pos == 0)
        return hpi_TRUE;

    LOG_I("\nCommitting %d bytes from swap to '%s' partition...", instance->swap_write_pos, instance->old_part->name);

    LOG_D("Erasing '%s' partition from offset %d...", instance->old_part->name, instance->committed_len);
    if (fal_partition_erase(instance->old_part, instance->committed_len, instance->swap_write_pos) < 0)
    {
        LOG_E("Failed to erase '%s' partition at offset %d.", instance->old_part->name, instance->committed_len);
        return hpi_FALSE;
    }

    if (flash_to_flash_copy(instance->swap_part, instance->swap_offset, instance->old_part, instance->committed_len, instance->swap_write_pos) < 0)
    {
        LOG_E("Failed to copy from swap to '%s' partition.", instance->old_part->name);
        return hpi_FALSE;
    }

    LOG_D("Erasing swap partition for next round...");
    if (fal_partition_erase(instance->swap_part, instance->swap_offset, instance->swap_size) < 0)
    {
        LOG_W("Failed to erase swap partition, may affect next write cycle.");
        return hpi_FALSE;
    }

    instance->committed_len += instance->swap_write_pos;
    instance->swap_write_pos = 0;
    LOG_I("\nCommit successful. Total committed: %d bytes.", instance->committed_len);
    return hpi_TRUE;
}

/**
 * @brief HPatchLite listener: Writes new data by buffering it in the swap partition.
 *        Triggers a commit when the buffer is full.
 * @param listener  Pointer to the listener instance.
 * @param data      Pointer to the new data to be written.
 * @param size      Size of the new data.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL _do_write_new_flash(struct hpatchi_listener_t *listener, const hpi_byte *data, hpi_size_t size)
{
    hpatchi_instance_t *instance = (hpatchi_instance_t *)listener;
    const hpi_byte *data_cur = data;
    hpi_size_t remain_size = size;

    // The amount of data written at one time may be very large, requiring processing in loops.
    while (remain_size > 0)
    {
        // Calculate the remaining space of the swap partition
        int swap_free_space = instance->swap_size - instance->swap_write_pos;
        // First, fill up the swap partition.
        if (swap_free_space < remain_size)
        {
            if (swap_free_space > 0)
            {
                if (fal_partition_write(instance->swap_part, instance->swap_offset + instance->swap_write_pos, (const uint8_t *)data_cur, swap_free_space) < 0)
                    return hpi_FALSE;
                instance->swap_write_pos += swap_free_space;
                data_cur += swap_free_space;
                remain_size -= swap_free_space;
            }
            //Execute submit operation
            if (!_commit_swap_to_old_flash(instance))
                return hpi_FALSE;
        }
        else
        {
            // Write all remaining data (which could be all the data or just a truncated portion) directly to the swap partition
            if (remain_size > 0)
            {
                if (fal_partition_write(instance->swap_part, instance->swap_offset + instance->swap_write_pos, (const uint8_t *)data_cur, remain_size) < 0)
                    return hpi_FALSE;
                instance->swap_write_pos += remain_size;
            }
            break;
        }
    }

    instance->newer_write_pos += size;

    int percent = instance->newer_write_pos * 100 / instance->newer_file_len;
    if (percent != instance->progress_percent && (percent % 5 == 0))
    {
        rt_kprintf("\rBuffering... %3d%%", percent);
        instance->progress_percent = percent;
    }
    return hpi_TRUE;
}

#endif // QBOOT_HPATCH_USE_FLASH_SWAP


// -----------------------------------------------------------------------------
// RAM Buffer Strategy Implementation
// -----------------------------------------------------------------------------
#if defined(QBOOT_HPATCH_USE_RAM_BUFFER)

/**
 * @brief Commits the buffered data from the RAM buffer to the old partition.
 *        This is the core of the "write-buffer, commit-full" logic for RAM.
 * @param instance Pointer to the patch instance.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL _commit_ram_to_old(hpatchi_instance_t *instance)
{
    if (instance->swap_write_pos == 0)
        return hpi_TRUE;

    LOG_I("\nCommitting %d bytes from RAM buffer to '%s' partition...", instance->swap_write_pos, instance->old_part->name);

    // 1. Erase the target area on the old partition
    LOG_D("Erasing '%s' partition from offset %d...", instance->old_part->name, instance->committed_len);
    if (fal_partition_erase(instance->old_part, instance->committed_len, instance->swap_write_pos) < 0)
    {
        LOG_E("Failed to erase '%s' partition at offset %d.", instance->old_part->name, instance->committed_len);
        return hpi_FALSE;
    }

    // 2. Write the entire RAM buffer content to the erased area
    if (fal_partition_write(instance->old_part, instance->committed_len, instance->swap_buffer, instance->swap_write_pos) < 0)
    {
        LOG_E("Failed to write from RAM buffer to '%s' partition.", instance->old_part->name);
        return hpi_FALSE;
    }

    // 3. Update state: only reset the write position, no need to clear RAM
    instance->committed_len += instance->swap_write_pos;
    instance->swap_write_pos = 0;
    LOG_I("\nCommit successful. Total committed: %d bytes.", instance->committed_len);
    return hpi_TRUE;
}

/**
 * @brief HPatchLite listener: Writes new data by buffering it in the RAM buffer.
 *        Triggers a commit when the buffer is full.
 * @param listener  Pointer to the listener instance.
 * @param data      Pointer to the new data to be written.
 * @param size      Size of the new data.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL _do_write_new_ram(struct hpatchi_listener_t *listener, const hpi_byte *data, hpi_size_t size)
{
    hpatchi_instance_t *instance = (hpatchi_instance_t *)listener;
    const hpi_byte *data_cur = data;
    hpi_size_t remain_size = size;

    while (remain_size > 0)
    {
        int swap_free_space = instance->swap_buffer_size - instance->swap_write_pos;
        if (swap_free_space < remain_size)
        {
            if (swap_free_space > 0)
            {
                rt_memcpy(instance->swap_buffer + instance->swap_write_pos, data_cur, swap_free_space);
                instance->swap_write_pos += swap_free_space;
                data_cur += swap_free_space;
                remain_size -= swap_free_space;
            }
            if (!_commit_ram_to_old(instance))
                return hpi_FALSE;
        }
        else
        {
            if (remain_size > 0)
            {
                rt_memcpy(instance->swap_buffer + instance->swap_write_pos, data_cur, remain_size);
                instance->swap_write_pos += remain_size;
            }
            break;
        }
    }

    instance->newer_write_pos += size;

    int percent = instance->newer_write_pos * 100 / instance->newer_file_len;
    if (percent != instance->progress_percent && (percent % 5 == 0))
    {
        rt_kprintf("\rBuffering... %3d%%", percent);
        instance->progress_percent = percent;
    }
    return hpi_TRUE;
}

#endif // QBOOT_HPATCH_USE_RAM_BUFFER


// -----------------------------------------------------------------------------
// Main Public Function
// -----------------------------------------------------------------------------

/**
 * @brief Performs an in-place differential update from a patch package.
 * 
 * This function orchestrates the entire patch process. It initializes the appropriate
 * buffer strategy (Flash or RAM), invokes the HPatchLite library, and handles
 * the finalization steps like committing the last block and erasing tail data.
 * 
 * @param patch_part        FAL partition containing the patch data.
 * @param old_part          FAL partition containing the old firmware, which will be updated in-place.
 * @param patch_file_len    Length of the patch data within the patch partition.
 * @param newer_file_len    Expected length of the new firmware after patching.
 * @param patch_file_offset Starting offset of the patch data within the patch partition.
 * @return 0 on success, non-zero on failure.
 */
int qbt_hpatchlite_release_from_part(const fal_partition_t patch_part, const fal_partition_t old_part, int patch_file_len, int newer_file_len, int patch_file_offset)
{
    hpatchi_instance_t instance = {
        .patch_part = patch_part,
        .old_part = old_part,
        .patch_file_offset = patch_file_offset,
        .patch_file_len = patch_file_len,
        .newer_file_len = newer_file_len,
        .progress_percent = -1,
    };
    hpi_patch_result_t result = HPATCHI_PATCH_ERROR;

#if defined(QBOOT_HPATCH_USE_FLASH_SWAP)
    LOG_I("HPatchLite: Using FLASH swap strategy.");
    instance.swap_part = (fal_partition_t)fal_partition_find(QBOOT_HPATCH_SWAP_PART_NAME);
    if (!instance.swap_part)
    {
        LOG_E("Swap partition '%s' not found!", QBOOT_HPATCH_SWAP_PART_NAME);
        return -1;
    }
    instance.swap_offset = QBOOT_HPATCH_SWAP_OFFSET;
    instance.swap_size = instance.swap_part->len - instance.swap_offset;

    LOG_I("Erasing swap area (size: %d) before use...", instance.swap_size);
    if (fal_partition_erase(instance.swap_part, instance.swap_offset, instance.swap_size) < 0)
    {
        LOG_E("Failed to erase swap partition! OTA aborted.");
        return -1;
    }

    result = hpi_patch(&instance.parent, QBOOT_HPATCH_PATCH_CACHE_SIZE, QBOOT_HPATCH_DECOMPRESS_CACHE_SIZE, _do_read_patch, _do_read_old, _do_write_new_flash);

    if (result == HPATCHI_SUCCESS && instance.swap_write_pos > 0)
    {
        if (!_commit_swap_to_old_flash(&instance))
            result = HPATCHI_PATCH_ERROR;
    }
#elif defined(QBOOT_HPATCH_USE_RAM_BUFFER)
    LOG_I("HPatchLite: Using RAM buffer strategy.");
    instance.swap_buffer_size = QBOOT_HPATCH_RAM_BUFFER_SIZE;
    instance.swap_buffer = rt_malloc(instance.swap_buffer_size);
    if (!instance.swap_buffer)
    {
        LOG_E("Failed to malloc %d bytes for RAM buffer.", instance.swap_buffer_size);
        return -1;
    }
    instance.swap_write_pos = 0;
    instance.committed_len = 0;
    LOG_D("Allocated %d bytes for RAM buffer.", instance.swap_buffer_size);

    result = hpi_patch(&instance.parent, QBOOT_HPATCH_PATCH_CACHE_SIZE, QBOOT_HPATCH_DECOMPRESS_CACHE_SIZE, _do_read_patch, _do_read_old, _do_write_new_ram);

    // After the patch loop, commit the last data block in RAM if it exists
    if (result == HPATCHI_SUCCESS && instance.swap_write_pos > 0)
    {
        if (!_commit_ram_to_old(&instance))
        {
            result = HPATCHI_PATCH_ERROR;
        }
    }
    rt_free(instance.swap_buffer);
#else
#error "No HPatchLite buffer strategy selected. Please define QBOOT_HPATCH_USE_FLASH_SWAP or QBOOT_HPATCH_USE_RAM_BUFFER."
#endif

    // --- Finalization: Tail Erase and Verification ---
    if (result == HPATCHI_SUCCESS)
    {
        const struct fal_flash_dev *flash_dev = fal_flash_device_find(old_part->flash_name);
        if (flash_dev && newer_file_len < old_part->len)
        {
            int sector_size = flash_dev->blk_size;
            int erase_start_addr = (newer_file_len % sector_size == 0) ? newer_file_len : ((newer_file_len / sector_size) + 1) * sector_size;

            if (erase_start_addr < old_part->len)
            {
                int erase_size = old_part->len - erase_start_addr;
                LOG_I("New firmware is smaller than partition. Erasing aligned tail data...");
                LOG_D("Erasing from aligned offset %d, size %d", erase_start_addr, erase_size);
                if (fal_partition_erase(old_part, erase_start_addr, erase_size) < 0)
                {
                    LOG_W("Failed to erase tail data, but patch itself is considered successful.");
                }
            }
        }
    }

    // Final check and result reporting
    if (result == HPATCHI_SUCCESS)
    {
        int final_len = 0;
        final_len = instance.committed_len;
        if (final_len == newer_file_len)
        {
            LOG_I("Update successful! Total size: %d bytes.", final_len);
        }
        else
        {
            LOG_E("Update finished, but final length (%d) != newer_file_len (%d)!",
                  final_len, newer_file_len);
            result = HPATCHI_PATCH_ERROR;
        }
    }
    else
    {
        LOG_E("Update failed with error code: %d", result);
    }

    return (result == HPATCHI_SUCCESS);
}

#endif // QBOOT_USING_HPATCHLITE
