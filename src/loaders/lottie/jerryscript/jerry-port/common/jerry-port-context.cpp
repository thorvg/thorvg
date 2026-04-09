/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <cstdlib>

#include "jcontext.h"


jerry_context_t* g_context_p = nullptr;
bool jerry_use_tls = false;

/**
 * Static buffer for the first context — BSS allocation.
 * Same cache/page characteristics as non-EXTERNAL_CONTEXT global context.
 */
alignas (JMEM_ALIGNMENT) char jerry_static_context_buffer[
    sizeof (jerry_context_t) + JERRY_GLOBAL_HEAP_SIZE * 1024];

/**
 * Thread-local context pointer for multi-threaded Lottie expression evaluation.
 * Each thread gets an independent jerry_context_t + heap via TLS.
 */
#if defined(__MINGW32__) || defined(__MINGW64__)

#include <windows.h>

DWORD tls_context_index = TLS_OUT_OF_INDEXES;

#elif defined(_MSC_VER)

__declspec(thread) jerry_context_t* tls_context_p = nullptr;

#else

__thread jerry_context_t* tls_context_p
    __attribute__((visibility("hidden"))) = nullptr;

#endif

#if defined(__MINGW32__) || defined(__MINGW64__)

static void jerry_port_tls_set(jerry_context_t* p)
{
    if (tls_context_index == TLS_OUT_OF_INDEXES)
    {
        tls_context_index = TlsAlloc();
    }
    TlsSetValue(tls_context_index, p);
}
#else
static inline void jerry_port_tls_set(jerry_context_t* p) { tls_context_p = p; }
#endif

size_t jerry_port_context_alloc(size_t context_size)
{
    size_t total_size = context_size + JERRY_GLOBAL_HEAP_SIZE * 1024;
    if (g_context_p == nullptr)
    {
        /* First context: use static BSS buffer (link-time constant address) */
        g_context_p = (jerry_context_t*) jerry_static_context_buffer;
        jerry_port_tls_set(g_context_p);
        return total_size;
    }
    /* Subsequent contexts: heap-allocate for multi-thread */
    jerry_use_tls = true;
    jerry_port_tls_set((jerry_context_t*)malloc(total_size));
    return total_size;
} /* jerry_port_context_alloc */

jerry_context_t* jerry_port_context_get(void)
{
#if defined(__MINGW32__) || defined(__MINGW64__)
    return (jerry_context_t*)TlsGetValue(tls_context_index);
#else
    return tls_context_p;
#endif
} /* jerry_port_context_get */

void jerry_port_context_free(void)
{
    if (g_context_p != nullptr)
    {
        /* Static buffer: just reset pointer, don't free */
        g_context_p = nullptr;
        jerry_port_tls_set(nullptr);
        return;
    }
#if defined(__MINGW32__) || defined(__MINGW64__)
    free(TlsGetValue(tls_context_index));
#else
    free(tls_context_p);
#endif
    jerry_port_tls_set(nullptr);
} /* jerry_port_context_free */

/* ThorVG-specific: set the active context on the current thread for cross-thread cleanup */
void jerry_port_context_set(jerry_context_t *context_p)
{
    if (g_context_p != nullptr)
    {
        g_context_p = context_p;
    }
    jerry_port_tls_set(context_p);
}
