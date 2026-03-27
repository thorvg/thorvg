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

#include "jerryscript-port.h"

#ifndef JERRY_GLOBAL_HEAP_SIZE
#define JERRY_GLOBAL_HEAP_SIZE (512)
#endif /* JERRY_GLOBAL_HEAP_SIZE */

#include <cstdlib>

/**
 * Thread-local context pointer for multi-threaded Lottie expression evaluation.
 * Each thread gets an independent jerry_context_t + heap via TLS.
 */
static thread_local jerry_context_t* tls_context_p = nullptr;

size_t JERRY_ATTR_WEAK
jerry_port_context_alloc(size_t context_size)
{
    if(tls_context_p) {
      abort();
    }
    size_t total_size = context_size + JERRY_GLOBAL_HEAP_SIZE * 1024;
    tls_context_p = (jerry_context_t*)calloc(1, total_size);
    if (tls_context_p == nullptr) {
        abort();
    }

    return total_size;
} /* jerry_port_context_alloc */

jerry_context_t* JERRY_ATTR_WEAK
jerry_port_context_get(void)
{
    if(tls_context_p == nullptr) {
      abort();
    }
    return tls_context_p;
} /* jerry_port_context_get */

void JERRY_ATTR_WEAK
jerry_port_context_free(void)
{
    free(tls_context_p);
    tls_context_p = nullptr;
} /* jerry_port_context_free */
