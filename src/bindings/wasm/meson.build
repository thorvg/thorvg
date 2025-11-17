if cc.get_id() == 'emscripten'
    # Determine which WASM binding to build
    if get_option('bindings').contains('wasm_canvaskit')
        # Canvas Kit binding: C API + minimal C++ engine wrapper
        # C API is already built as part of thorvg_lib_dep (no need to add it again)
        source_file = [
          'tvgWasmCanvasKit.cpp'
        ]

        thorvg_wasm_dep = declare_dependency(
            include_directories: include_directories('.', '../capi', '../../common'),
            sources: source_file
        )

        executable('thorvg-canvaskit',
            [],
            include_directories: headers,
            dependencies: [thorvg_lib_dep, thorvg_wasm_dep],
        )
    elif get_option('bindings').contains('wasm_beta')
        # Original Lottie Animation binding
        source_file = [
          'tvgWasmDefaultFont.h',
          'tvgWasmLottieAnimation.cpp'
        ]
        thorvg_wasm_dep = declare_dependency(
            include_directories: include_directories('.'),
            sources: source_file
        )
        executable('thorvg',
            [],
            include_directories: headers,
            dependencies: [thorvg_lib_dep, thorvg_wasm_dep],
        )
    endif
else
    message('wasm compiler is not the emscripten!')
endif
