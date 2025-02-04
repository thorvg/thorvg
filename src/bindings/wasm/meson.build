if cc.get_id() == 'emscripten'
    source_file = files('tvgWasmLottieAnimation.cpp')
    thorvg_wasm_dep = declare_dependency(include_directories
                                         : include_directories('.'), sources
                                         : source_file)
    executable('thorvg-wasm',
        [],
        include_directories : headers,
        cpp_args : ['-s', 'MAIN_MODULE=1'],
        link_args : ['-s', 'MAIN_MODULE=1', '--bind', '-sWASM=1', '-sALLOW_MEMORY_GROWTH=1', '-sEXPORT_ES6=1', '-sFORCE_FILESYSTEM=1', '-sMODULARIZE=1', '-sEXPORTED_RUNTIME_METHODS=FS', '-sUSE_WEBGPU=1', '-sSTACK_SIZE=2MB', '-sMAX_WEBGL_VERSION=2', '-sFULL_ES3'],
        dependencies : [thorvg_lib_dep, thorvg_wasm_dep],
        )
else
    message('wasm compiler is not the emscripten!')
endif
