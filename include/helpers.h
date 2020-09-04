#ifndef __HELPERS_H__
#define __HELPERS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Use in a struct: creates a name_buf and a name_size variable. */
#define BUF_VAR(type, name) \
  type * name##_buf;        \
  size_t name##_size;
/* this filles in `&structptr->something_buf, &structptr->something_size`. */
#define BUF_PARAMS(struct, name) \
  (void **)&struct->name##_buf, &struct->name##_size

#if __GNUC__ < 6
  #ifndef likely
    #define likely(_x) (_x)
  #endif
  #ifndef unlikely
    #define unlikely(_x) (_x)
  #endif
#else
  #ifndef likely
    #define likely(_x) __builtin_expect(!!(_x), 1)
  #endif
  #ifndef unlikely
    #define unlikely(_x) __builtin_expect(!!(_x), 0)
  #endif
#endif

/* Initial size used for ck_maybe_grow */
#define INITIAL_GROWTH_SIZE (64)

/* This function calculates the next power of 2 greater or equal its argument.
 @return The rounded up power of 2 (if no overflow) or 0 on overflow.
*/
static inline size_t next_pow2(size_t in) {

  if (in == 0 || in > (size_t)-1) {

    return 0;                  /* avoid undefined behaviour under-/overflow */

  }

  size_t out = in - 1;
  out |= out >> 1;
  out |= out >> 2;
  out |= out >> 4;
  out |= out >> 8;
  out |= out >> 16;
  return out + 1;

}

/* This function makes sure *size is > size_needed after call.
 It will realloc *buf otherwise.
 *size will grow exponentially as per:
 https://blog.mozilla.org/nnethercote/2014/11/04/please-grow-your-buffers-exponentially/
 Will return NULL and free *buf if size_needed is <1 or realloc failed.
 @return For convenience, this function returns *buf.
 */
static inline void *maybe_grow(void **buf, size_t *size, size_t size_needed) {

  /* No need to realloc */
  if (likely(size_needed && *size >= size_needed)) { return *buf; }

  /* No initial size was set */
  if (size_needed < INITIAL_GROWTH_SIZE) size_needed = INITIAL_GROWTH_SIZE;

  /* grow exponentially */
  size_t next_size = next_pow2(size_needed);

  /* handle overflow and zero size_needed */
  if (!next_size) next_size = size_needed;

  /* alloc */
  *buf = realloc(*buf, next_size);
  *size = *buf ? next_size : 0;

  return *buf;

}

#undef INITIAL_GROWTH_SIZE

#ifdef __cplusplus
}
#endif

#endif
