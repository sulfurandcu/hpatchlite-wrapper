# hpatchlite-wrapper

A lightweight and user-friendly wrapper for the [HPatchLite](https://github.com/sisong/HPatchLite).

## API

```c
hpi_patch_result_t hpi_patch(hpatchi_listener_t *listener,
                             int patch_cache_size,
                             int decompress_cache_size,
                             hpi_TInputStream_read _do_read_diff,
                             read_old_t _do_read_old,
                             write_new_t _do_write_new);
```

## Example

```c
typedef struct hpatchi_instance_t
{
    hpatchi_listener_t parent;

    // define your own variable data here
    int var1;
    int var2;
    int var3;

} hpatchi_instance_t;

hpi_BOOL _do_read_empty(struct hpatchi_listener_t *listener, hpi_pos_t addr, hpi_byte *data, hpi_size_t size)
{
    ...
}

hpi_BOOL _do_read_old(struct hpatchi_listener_t *listener, hpi_pos_t addr, hpi_byte *data, hpi_size_t size)
{
    ...
}

hpi_BOOL _do_read_patch(hpi_TInputStreamHandle input_stream, hpi_byte *data, hpi_size_t *size)
{
    ...
}

hpi_BOOL _do_write_new(struct hpatchi_listener_t *listener, const hpi_byte *data, hpi_size_t size)
{
    ...
}

void demo(void)
{
    hpatchi_instance_t instance = {0};
    instance.var1 = ;
    instance.var2 = ;
    instance.var3 = ;

    // 差分全量升级 (full update with hpatchlite)
    {
        hpi_patch(&instance.parent, 128, 128, _do_read_patch, _do_read_empty, _do_write_new);
    }

    // 差分增量升级 (diff update with hpatchlite)
    {
        hpi_patch(&instance.parent, 128, 128, _do_read_patch, _do_read_old, _do_write_new);
    }
}
```

## Contact

- sulfurandcu@gmail.com
- https://github.com/sulfurandcu/hpatchlite-wrapper

## Reference

-  https://sulfurandcu.github.io/sulfurandcu.io/cloidefbf00hzv0rqa7zg722r.html
