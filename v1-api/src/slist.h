/*
 * Copyright (c) 2011-2015 Intel Corporation.  All rights reserved.
 * Copyright (c) 2016 Cray Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef SLIST_H
#define SLIST_H

#include <stdlib.h>
#include <sys/types.h>

/*
 * Single-linked list
 */
struct slist_entry {
    struct slist_entry *next;
};

struct slist {
    struct slist_entry *head;
    struct slist_entry *tail;
};

static inline void slist_init(struct slist *list) {
    list->head = list->tail = NULL;
}

static inline int slist_empty(struct slist *list) { return !list->head; }

static inline void slist_insert_head(struct slist_entry *item,
                                     struct slist *list) {
    if (slist_empty(list)) {
        list->tail = item;
        item->next = NULL;
    } else {
        item->next = list->head;
    }

    list->head = item;
}

static inline void slist_insert_tail(struct slist_entry *item,
                                     struct slist *list) {
    if (slist_empty(list))
        list->head = item;
    else
        list->tail->next = item;

    item->next = NULL;
    list->tail = item;
}

static inline struct slist_entry *slist_remove_head(struct slist *list) {
    struct slist_entry *item;

    item = list->head;
    if (list->head == list->tail)
        slist_init(list);
    else
        list->head = item->next;
#if ENABLE_DEBUG
    if (item) {
        item->next = NULL;
    }
#endif
    return item;
}

#define slist_foreach(list, item, prev)                                        \
    for ((prev) = NULL, (item) = (list)->head; (item);                         \
         (prev) = (item), (item) = (item)->next)

#define slist_remove_head_container(list, type, container, member)             \
    do {                                                                       \
        if (slist_empty(list)) {                                               \
            container = NULL;                                                  \
        } else {                                                               \
            container = container_of((list)->head, type, member);              \
            slist_remove_head(list);                                           \
        }                                                                      \
    } while (0)

typedef int slist_func_t(struct slist_entry *item, const void *arg);

static inline struct slist_entry *
slist_find_first_match(const struct slist *list, slist_func_t *match,
                       const void *arg) {
    struct slist_entry *item;
    for (item = list->head; item; item = item->next) {
        if (match(item, arg))
            return item;
    }

    return NULL;
}

static inline void slist_insert_before_first_match(struct slist *list,
                                                   slist_func_t *match,
                                                   struct slist_entry *entry) {
    struct slist_entry *cur, *prev;

    slist_foreach(list, cur, prev) {
        if (match(cur, entry)) {
            if (!prev) {
                slist_insert_head(entry, list);
            } else {
                entry->next = prev->next;
                prev->next = entry;
            }
            return;
        }
    }
    slist_insert_tail(entry, list);
}

static inline void slist_remove(struct slist *list, struct slist_entry *item,
                                struct slist_entry *prev) {
    if (prev)
        prev->next = item->next;
    else
        list->head = item->next;

    if (!item->next)
        list->tail = prev;
}

static inline struct slist_entry *slist_remove_first_match(struct slist *list,
                                                           slist_func_t *match,
                                                           const void *arg) {
    struct slist_entry *item, *prev;

    slist_foreach(list, item, prev) {
        if (match(item, arg)) {
            slist_remove(list, item, prev);
            return item;
        }
    }

    return NULL;
}

static inline void slist_swap(struct slist *dst, struct slist *src) {
    struct slist_entry *dst_head = dst->head;
    struct slist_entry *dst_tail = dst->tail;

    dst->head = src->head;
    dst->tail = src->tail;

    src->head = dst_head;
    src->tail = dst_tail;
}

#endif /* SLIST_H */