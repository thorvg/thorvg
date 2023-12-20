if (cc.get_id() == 'emscripten')
    source_file = files('tvgWasmLottieAnimation.cpp')
    thorvg_wasm_dep = declare_dependency(include_directories
                                         : include_directories('.'), sources
                                         : source_file)
    executable('thorvg-wasm',
        [],
        include_directories : headers,
        dependencies : [thorvg_lib_dep, thorvg_wasm_dep],
        )
else
    message('wasm compiler is not the emscripten!')
endif
