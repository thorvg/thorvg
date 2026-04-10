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

/**
 * Allocator implementation
 */
#include "ecma-globals.h"

#include "jcontext.h"
#include "jmem.h"
#include "jrt-libc-includes.h"

#define JMEM_ALLOCATOR_INTERNAL
#include "jmem-allocator-internal.h"

/**
 * Initialize memory allocators.
 */
void
jmem_init (void)
{
  jmem_heap_init ();
} /* jmem_init */

/**
 * Finalize memory allocators.
 */
void
jmem_finalize (void)
{
  jmem_pools_finalize ();
  jmem_heap_finalize ();
} /* jmem_finalize */

/**
 * Compress pointer
 *
 * @return packed pointer
 */
jmem_cpointer_t JERRY_ATTR_PURE
jmem_compress_pointer (const void *pointer_p) /**< pointer to compress */
{
  JERRY_DEFINE_CURRENT_CONTEXT ();
  return jmem_compress_pointer_from_context (jerry_current_context_p, pointer_p);
} /* jmem_compress_pointer */

/**
 * Decompress pointer
 *
 * @return unpacked pointer
 */
void *JERRY_ATTR_PURE
jmem_decompress_pointer (uintptr_t compressed_pointer) /**< pointer to decompress */
{
  JERRY_DEFINE_CURRENT_CONTEXT ();
  return jmem_decompress_pointer_from_context (jerry_current_context_p, compressed_pointer);
} /* jmem_decompress_pointer */
