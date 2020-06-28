#ifndef __COMMON_H__
#define __COMMON_H__

/* Use in a struct: creates a name_buf and a name_size variable. */
#define BUF_VAR(type, name) \
  type * name##_buf;        \
  size_t name##_size;
/* this filles in `&structptr->something_buf, &structptr->something_size`. */
#define BUF_PARAMS(struct, name) \
  (void **)&struct->name##_buf, &struct->name##_size

#endif
