#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

typedef int
    __attribute__((nonnull(1, 2))) (*list_cmp_func_t)(const struct list_head *,
                                                      const struct list_head *);

/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */


/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *head = malloc(sizeof(struct list_head));
    if (head)
        INIT_LIST_HEAD(head);
    return head;
}

/* Free all storage used by queue */
void q_free(struct list_head *head)
{
    if (!head)
        return;

    element_t *ptr, *safe;
    list_for_each_entry_safe (ptr, safe, head, list) {
        q_release_element(ptr);
    }

    free(head);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head)
        return false;

    element_t *node = malloc(sizeof(element_t));
    if (!node)
        return false;
    size_t len = strlen(s);
    node->value = malloc(len + 1);
    if (!node->value) {
        free(node);
        return false;
    }

    memcpy(node->value, s, len + 1);

    list_add(&node->list, head);
    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    return q_insert_head(head->prev, s);
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    element_t *node = list_entry(head->next, element_t, list);
    list_del(&node->list);
    if (sp) {
        memcpy(sp, node->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }
    return node;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    return q_remove_head(head->prev->prev, sp, bufsize);
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;

    int len = 0;
    struct list_head *li;

    list_for_each (li, head)
        len++;

    return len;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    // https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/
    if (!head || list_empty(head))
        return false;
    struct list_head *slow = head->next, *fast = head->next->next;
    while (fast != head && fast->next != head) {
        fast = fast->next->next;
        slow = slow->next;
    }
    list_del_init(slow);
    q_release_element(list_entry(slow, element_t, list));
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    if (!head)
        return false;
    struct list_head *node = head;
    while (node->next != head) {
        struct list_head *ptr = node->next;
        int cnt = 1;
        while (ptr->next != head) {
            element_t *curr = list_entry(ptr, element_t, list);
            element_t *next = list_entry(ptr->next, element_t, list);
            if (strcmp(curr->value, next->value) == 0) {
                ptr = ptr->next;
                cnt++;
            } else
                break;
        }

        // found duplicated element
        if (cnt > 1) {
            while (cnt--) {
                element_t *tmp = list_entry(node->next, element_t, list);
                list_del(&tmp->list);
                q_release_element(tmp);
            }
        } else
            node = node->next;
    }
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    // https://leetcode.com/problems/swap-nodes-in-pairs/
    if (!head || list_is_singular(head))
        return;
    struct list_head *node;
    for (node = head->next; node != head && node->next != head;
         node = node->next) {
        list_del(node);
        list_add(node, node->next);
    }
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_is_singular(head))
        return;

    struct list_head *node = head->next;
    while (node->next != head) {
        struct list_head *next = node->next;
        list_del(next);
        list_add(next, head);
    }
}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    // https://leetcode.com/problems/reverse-nodes-in-k-group/
    if (!head || list_is_singular(head))
        return;

    int count = q_size(head);

    struct list_head *node = head;
    struct list_head *curr, *next;
    while (count >= k) {
        curr = node->next;
        for (int i = 0; i < k - 1; i++) {
            next = curr->next;
            list_del(next);
            list_add(next, node);
        }
        count -= k;
        node = curr;
    }
}

static inline int q_less(const struct list_head *a, const struct list_head *b)
{
    return strcmp(list_entry(a, element_t, list)->value,
                  list_entry(b, element_t, list)->value);
}

static inline int q_greater(const struct list_head *a,
                            const struct list_head *b)
{
    return strcmp(list_entry(b, element_t, list)->value,
                  list_entry(a, element_t, list)->value);
}

static void merge(struct list_head **head,
                  struct list_head *a,
                  struct list_head *b,
                  list_cmp_func_t cmp)
{
    struct list_head **tail = head;

    while (a && b) {
        if (cmp(a, b) <= 0) {
            *tail = a;
            a = a->next;
        } else {
            *tail = b;
            b = b->next;
        }

        tail = &(*tail)->next;
    }

    if (a) {
        *tail = a;
    } else if (b) {
        *tail = b;
    }
}

/* Sort elements of queue in ascending order */
void q_sort(struct list_head *head, bool descend)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;

    list_cmp_func_t q_cmp = descend ? q_greater : q_less;

    // Split original list into sorted sublists.
    struct list_head *h = head->next, *t = head->next;
    struct list_head *sublist = NULL;
    while (h != head) {
        while (t->next != head && q_cmp(t, t->next) <= 0) {
            t = t->next;
        }
        h->prev = sublist;
        sublist = h;
        t = t->next;
        t->prev->next = NULL;
        h = t;
    }

    // Merge sublists.
    struct list_head *tmp, *currsub;
    while (sublist->prev) {
        struct list_head **subhead;

        currsub = sublist;
        subhead = &sublist;
        while (currsub && currsub->prev) {
            tmp = currsub->prev->prev;
            merge(subhead, currsub, currsub->prev, q_cmp);
            subhead = &(*subhead)->prev;
            currsub = tmp;
        }
        *subhead = currsub;
    }
    // Fix doubly linked list
    head->next = sublist;
    tmp = head;
    while (sublist) {
        sublist->prev = tmp;
        tmp = sublist;
        sublist = sublist->next;
    }
    head->prev = tmp;
    tmp->next = head;
}


static inline int q_remove_cmp(struct list_head *head, list_cmp_func_t cmp)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    if (!head || list_empty(head) || list_is_singular(head))
        return q_size(head);

    struct list_head *node = head->prev;
    while (node != head && node->prev != head) {
        if (cmp(node, node->prev) < 0) {
            struct list_head *tmp = node->prev;
            list_del_init(tmp);
            q_release_element(list_entry(tmp, element_t, list));
        } else
            node = node->prev;
    }
    return q_size(head);
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    return q_remove_cmp(head, q_greater);
}

int q_ascend(struct list_head *head)
{
    return q_remove_cmp(head, q_less);
}

static inline int get_contex_id(struct list_head *head)
{
    return list_entry(head, queue_contex_t, chain)->id;
}

/* Merge all the queues into one sorted queue, which is in ascending order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/
    int n_queue = 0;
    queue_contex_t *ptr = NULL;
    list_for_each_entry (ptr, head, chain) {
        n_queue++;
        ptr->q->prev->next = NULL;
    }

    list_cmp_func_t q_cmp = descend ? q_greater : q_less;

    struct list_head *l1, *l2;
    struct list_head *contex1, *contex2;
    for (int interval = 1; interval < n_queue; interval *= 2) {
        contex1 = head->next;
        contex2 = head->next;
        for (int i = 0; i < interval; i++)
            contex2 = contex2->next;

        while (get_contex_id(contex1) + interval < n_queue) {
            l1 = list_entry(contex1, queue_contex_t, chain)->q;
            l2 = list_entry(contex2, queue_contex_t, chain)->q;

            struct list_head *subhead = l1->next;
            struct list_head **indir = &subhead;
            merge(indir, subhead, l2->next, q_cmp);
            l1->next = *indir;
            INIT_LIST_HEAD(l2);
            if (get_contex_id(contex1) + 2 * interval > n_queue)
                break;
            for (int i = 0; i < interval * 2; i++) {
                contex1 = contex1->next;
                contex2 = contex2->next;
            }
        }
    }

    // Fix doubly linked list
    l1 = list_entry(head->next, queue_contex_t, chain)->q;
    struct list_head *prev = l1, *curr = l1->next;
    while (curr) {
        curr->prev = prev;
        prev = curr;
        curr = curr->next;
    }
    l1->prev = prev;
    prev->next = l1;
    return q_size(l1);
}
