# hpatchlite-wrapper

一个为 [HPatchLite](https://github.com/sisong/HPatchLite) 设计的、轻量级且用户友好的封装层。

## 项目简介

本项目提供了一个简化的API，旨在帮助开发者将功能强大的 `HPatchLite` 二进制差分库快速集成到嵌入式系统中，尤其是在 RT-Thread 生态下。本封装层处理了复杂的内存管理和不同解压插件的抽象，让您能更专注于上层业务逻辑。

## 使用说明
- 本项目使用 `HPatchLite` 作为Git子模块。在克隆主仓库后，请使用以下命令来拉取子模块的完整代码：
> git submodule update --init --recursive

## API 接口

```c
/**
 * @brief 使用 HPatchLite 库执行差分合并操作
 * 
 * @param listener              指向监听器结构的指针，同时也作为用户上下文句柄。
 * @param patch_cache_size      用于核心差分算法的内部缓存大小。
 * @param decompress_cache_size 用于解压数据流的缓存大小。
 * @param _do_read_diff         读取差分补丁数据流的回调函数。
 * @param _do_read_old          读取旧版本固件数据的回调函数。
 * @param _do_write_new         写入新合成的固件数据的回调函数。
 * @return hpi_patch_result_t   成功时返回 HPATCHI_SUCCESS，失败时返回对应的错误码。
 */
hpi_patch_result_t hpi_patch(hpatchi_listener_t *listener,
                             int patch_cache_size,
                             int decompress_cache_size,
                             hpi_TInputStream_read _do_read_diff,
                             read_old_t _do_read_old,
                             write_new_t _do_write_new);
```

## 使用示例

- 参考`hpatch_demo.c`

```c
#include "hpatch_impl.h"

// 定义一个上下文结构体，用于保存所有必要的状态和数据
typedef struct hpatchi_instance_t
{
    hpatchi_listener_t parent; // 必须作为第一个成员

    // 在此处定义您自己的状态变量
    const fal_partition_t patch_part;
    const fal_partition_t old_part;
    int patch_read_pos;
    // ... 等等

} hpatchi_instance_t;

// 实现所需的回调函数
hpi_BOOL _do_read_old(struct hpatchi_listener_t *listener, hpi_pos_t addr, hpi_byte *data, hpi_size_t size)
{
    // 实现从旧固件分区读取的逻辑
    ...
}

hpi_BOOL _do_read_patch(hpi_TInputStreamHandle input_stream, hpi_byte *data, hpi_size_t *size)
{
    // 实现从差分补丁数据流读取的逻辑
    ...
}

hpi_BOOL _do_write_new(struct hpatchi_listener_t *listener, const hpi_byte *data, hpi_size_t size)
{
    // 实现原地升级的逻辑 (例如，使用交换缓冲区)
    ...
}

void demo_patch_update(void)
{
    // 初始化您的实例，包含所有必要的分区信息和状态
    hpatchi_instance_t instance = {
        .patch_part = fal_partition_find("download"),
        .old_part = fal_partition_find("app"),
        .patch_read_pos = 0,
        // ... 初始化其他成员
    };

    // 使用您的实例和回调函数来调用差分合并接口
    hpi_patch_result_t result = hpi_patch(&instance.parent, 
                                          4096, // patch_cache_size
                                          4096, // decompress_cache_size
                                          _do_read_patch, 
                                          _do_read_old, 
                                          _do_write_new);
    
    if (result == HPATCHI_SUCCESS) {
        // 差分合并成功!
    } else {
        // 差分合并失败，处理错误
    }
}
```

## 注意事项
- `hpatch-lite`具有另一个api`hpatchi_inplaceB`原地升级功能.使用demo中的swap功能效果一致,但是需要打包时进行额外的选项打包.**暂未支持**

## 补丁文件结构

由 `hdiffi` 工具生成的差分补丁文件主要由两部分构成：HPatchLite 主文件头，以及紧随其后的数据流。数据流本身根据压缩类型的不同，可能还会包含自己的子文件头。

### 原始十六进制数据示例

```
68 49 01 53 10 23 02 36 cd b9 69 00 00 ff 03 82 ...
```

### 逐字节分析

文件被按顺序解析。

#### 1. HPatchLite 主文件头 (由 `hpatch_lite_open` 解析)

| 字节 | 十六进制 | 值/ASCII | 解释                                                                                                                                                             |
|:-----|:---------|:---------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 0    | `68`     | `'h'`    | **HPatchLite 魔法数 1**                                                                                                                                          |
| 1    | `49`     | `'I'`    | **HPatchLite 魔法数 2**                                                                                                                                          |
| 2    | `01`     | `1`      | **压缩类型 (Compress Type)**：`1` 对应 `hpi_compressType_tuz`，表示本补丁使用了 `tuz` 压缩。                                                                     |
| 3    | `53`     | `83`     | **版本与长度编码字节**：二进制为 `01010011`。<br>- **高2位 (`01`)**: 版本号 `1`。<br>- **中3位 (`010`)**: `uncompressSize` 字段占 **2** 个字节。<br>- **低3位 (`011`)**: `newSize` 字段占 **3** 个字节。 |
| 4-6  | `10 23 02` | -        | **newSize (3 字节)**：小端（Little-Endian）编码的值。<br>`0x022310` = **140048**。这个值是最终生成的新固件 `new.bin` 的总大小。                               |
| 7-8  | `36 cd`  | -        | **uncompressSize (2 字节)**：小端编码。<br>`0xCD36` = **52534**。这个值是未经压缩的、原始的差分指令流的大小。                                                    |

**结论**: 主文件头的长度是**可变的**。解析完这部分后，数据流指针将指向下一个部分，准备交给相应的解压器处理。

#### 2. 解压器子文件头 (以 `tuz` 为例)

紧跟在主文件头之后的数据属于特定的解压器。对于 `tuz` 来说，这部分以字典大小信息开头。

| 字节 | 十六进制 | 值       | 解释                                                                                                                                                            |
|:-----|:---------|:---------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 9-10 | `b9 69`  | -        | **字典大小信息**: 这是 `tuz` 专有的头部数据，由 `_tuz_TStream_getReservedMemSize` 函数读取。在此示例中，小端编码的 `0x69B9` 等于 **27065**。这个值是解压器**实际需要的总内存**，不一定等于您在打包时指定的字典大小。 |

## 压缩功能

为了显著减小补丁包的体积，可以使用 `-c` 参数在生成补丁时启用压缩功能。

### 使用 `tuz` 压缩

`tuz` 是一种轻量级压缩算法，非常适合在嵌入式系统上使用。

*   **打包示例**:
    以下命令创建了一个使用 `tuz` 压缩的补丁。由于未指定字典大小，`hdiffi` 会使用默认的 **32KB** 字典。

    ```sh
    # -c-tuz 参数启用了 tuz 压缩
    .\hdiffi.exe -c-tuz .\old.bin .\new.bin .\patch_tuz_de.bin
    ```

*   **输出日志解读**:
    ```sh
    old : ".\old.bin"
    new : ".\new.bin"
    out : ".\patch_tuz_de.bin"
    hdiffi run with compress plugin: "tuz"
    oldDataSize : 140048
    newDataSize : 140048
      (used one tinyuz dictSize: 32768  (input data: 52534))
    diffDataSize: 76
    ...
    hpatchi run with decompresser: "tuz"
      requirements memory size: (must) 27065 + (custom cache) 32768
    ...
    ```

*   **关键信息解读**:
    1.  `used one tinyuz dictSize: 32768`: 这行日志确认了PC端的压缩器确实按照要求，**使用了32KB的窗口**来寻找重复数据。
    2.  `requirements memory size: (must) 27065`: **这是对嵌入式设备至关重要的信息。** 它指明了在单片机上执行解压时，`tuz` 解压器将通过 `hpi_malloc` **强制要求分配的RAM大小**。这个大小包括了字典缓冲区和解压器自身需要的状态内存。
    3.  **结论**: 打包时指定的字典大小 (`-dictSize=32k`) 是一个**上限和参考**。设备端实际需要的内存（`27065`）是由压缩后的数据流本身决定的。您**必须确保**您的设备有足够的堆内存来满足这个 "must" 的要求，否则升级将会因内存分配失败而终止。

## 联系方式

- sulfurandcu@gmail.com
- https://github.com/sulfurandcu/hpatchlite-wrapper

## 参考链接

-  https://sulfurandcu.github.io/sulfurandcu.io/cloidefbf00hzv0rqa7zg722r.html