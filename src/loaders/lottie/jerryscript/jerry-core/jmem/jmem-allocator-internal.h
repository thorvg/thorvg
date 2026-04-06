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

#ifndef JMEM_ALLOCATOR_INTERNAL_H
#define JMEM_ALLOCATOR_INTERNAL_H

#ifndef JMEM_ALLOCATOR_INTERNAL
#error "The header is for internal routines of memory allocator component. Please, don't use the routines directly."
#endif /* !JMEM_ALLOCATOR_INTERNAL */

/** \addtogroup mem Memory allocation
 * @{
 */

/**
 * @{
 * Valgrind-related options and headers
 */
#define JMEM_VALGRIND_NOACCESS_SPACE(p, s)
#define JMEM_VALGRIND_UNDEFINED_SPACE(p, s)
#define JMEM_VALGRIND_DEFINED_SPACE(p, s)
#define JMEM_VALGRIND_MALLOCLIKE_SPACE(p, s)
#define JMEM_VALGRIND_RESIZE_SPACE(p, o, n)
#define JMEM_VALGRIND_FREELIKE_SPACE(p)
/** @} */

void jmem_heap_init (void);
void jmem_heap_finalize (void);
bool jmem_is_heap_pointer (const void *pointer);
void *jmem_heap_alloc_block_internal (const size_t size);
void jmem_heap_free_block_internal (void *ptr, const size_t size);
void jmem_heap_free_block_internal_with_context (jerry_context_t *jerry_current_context_p, void *ptr, const size_t size);

static inline jmem_cpointer_t JERRY_ATTR_PURE
jmem_compress_pointer_from_context (jerry_context_t *jerry_current_context_p,
                                    const void *pointer_p)
{
  JERRY_ASSERT (pointer_p != NULL);
  JERRY_ASSERT (jmem_is_heap_pointer (pointer_p));

  uintptr_t uint_ptr = (uintptr_t) pointer_p;

  JERRY_ASSERT (uint_ptr % JMEM_ALIGNMENT == 0);

  const uintptr_t heap_start = (uintptr_t) &JERRY_HEAP_CONTEXT (first);

  uint_ptr -= heap_start;
  uint_ptr >>= JMEM_ALIGNMENT_LOG;

  JERRY_ASSERT (uint_ptr <= UINT16_MAX);
  JERRY_ASSERT (uint_ptr != JMEM_CP_NULL);

  return (jmem_cpointer_t) uint_ptr;
}

static inline void *JERRY_ATTR_PURE
jmem_decompress_pointer_from_context (jerry_context_t *jerry_current_context_p,
                                      uintptr_t compressed_pointer)
{
  JERRY_ASSERT (compressed_pointer != JMEM_CP_NULL);

  uintptr_t uint_ptr = compressed_pointer;

  JERRY_ASSERT (((jmem_cpointer_t) uint_ptr) == uint_ptr);

  const uintptr_t heap_start = (uintptr_t) &JERRY_HEAP_CONTEXT (first);

  uint_ptr <<= JMEM_ALIGNMENT_LOG;
  uint_ptr += heap_start;

  JERRY_ASSERT (jmem_is_heap_pointer ((void *) uint_ptr));

  return (void *) uint_ptr;
}

/**
 * \addtogroup poolman Memory pool manager
 * @{
 */

void jmem_pools_finalize (void);

/**
 * @}
 * @}
 */

/**
 * @{
 * Jerry mem-stat definitions
 */
#define JMEM_HEAP_STAT_INIT()
#define JMEM_HEAP_STAT_ALLOC(v1) JERRY_UNUSED (v1)
#define JMEM_HEAP_STAT_FREE(v1)  JERRY_UNUSED (v1)

/** @} */

#endif /* !JMEM_ALLOCATOR_INTERNAL_H */
