//decompresser_demo.h
// decompresser demo for HPatchLite
//
/*
 The MIT License (MIT)
 Copyright (c) 2020-2022 HouSisong All Rights Reserved.
 */
#ifndef hpatchi_decompresser_demo_h
#define hpatchi_decompresser_demo_h


#define _CompressPlugin_tuz


#ifndef _IsNeedIncludeDefaultCompressHead
#   define _IsNeedIncludeDefaultCompressHead 1
#endif


#ifdef _CompressPlugin_tuz
#if (_IsNeedIncludeDefaultCompressHead)
#   include "tuz_dec.h"  // "tinyuz/decompress/tuz_dec.h" https://github.com/sisong/tinyuz
#endif
    static size_t _tuz_TStream_getReservedMemSize(hpi_TInputStreamHandle codeStream,hpi_TInputStream_read readCode){
        const tuz_size_t dictSize=tuz_TStream_read_dict_size(codeStream,readCode);
        if (((tuz_size_t)(dictSize-1))>=tuz_kMaxOfDictSize)
            return 0;//error
        return dictSize;
    }
    static hpi_BOOL _tuz_TStream_decompress(hpi_TInputStreamHandle diffStream,hpi_byte* out_part_data,hpi_size_t* data_size){
        return tuz_STREAM_END>=tuz_TStream_decompress_partial((tuz_TStream*)diffStream,out_part_data,data_size);
    }
#endif //_CompressPlugin_tuz

#endif //hpatchi_decompresser_demo_h
