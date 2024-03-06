#ifndef LAB0_LIST_SORT_H
#define LAB0_LIST_SORT_H
#include <stdbool.h>
#include <stdint.h>
#include "list.h"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define u8 uint8_t

typedef int
    __attribute__((nonnull(2, 3))) (*list_cmp_func_t)(void *,
                                                      const struct list_head *,
                                                      const struct list_head *);



__attribute__((nonnull(2))) void list_sort(void *priv,
                                           struct list_head *head,
                                           bool descend);
#endif