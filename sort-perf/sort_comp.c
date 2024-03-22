#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "../list_sort.h"
#include "../queue.h"
#include "../random.h"

#define LOGN 7

static int sort = 0;
static int random_data = 1;
static int n_data = 128;
static int n_tests = 1;
static int descend = 0;

static int n_comp = 0;
static double exec_time = 0;
static double k = 0;

#define MIN_RANDSTR_LEN 5
#define MAX_RANDSTR_LEN 10
static const char charset[] = "abcdefghijklmnopqrstuvwxyz";
static char *file_path = NULL;

static inline int q_less(void *priv,
                         const struct list_head *a,
                         const struct list_head *b)
{
    element_t *elem_a = list_entry(a, element_t, list);
    element_t *elem_b = list_entry(b, element_t, list);
    return strcmp(elem_a->value, elem_b->value);
}

static inline int q_greater(void *priv,
                            const struct list_head *a,
                            const struct list_head *b)
{
    return q_less(priv, b, a);
}

__attribute__((nonnull(2, 3, 4))) static struct list_head *
merge(void *priv, list_cmp_func_t cmp, struct list_head *a, struct list_head *b)
{
    struct list_head *head = NULL, **tail = &head;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            *tail = a;
            tail = &a->next;
            a = a->next;
            n_comp++;
            if (!a) {
                *tail = b;
                break;
            }
        } else {
            *tail = b;
            tail = &b->next;
            b = b->next;
            n_comp++;
            if (!b) {
                *tail = a;
                break;
            }
        }
    }
    return head;
}

__attribute__((nonnull(2, 3, 4, 5))) static void merge_final(
    void *priv,
    list_cmp_func_t cmp,
    struct list_head *head,
    struct list_head *a,
    struct list_head *b)
{
    struct list_head *tail = head;
    u8 count = 0;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            tail->next = a;
            a->prev = tail;
            tail = a;
            a = a->next;
            n_comp++;
            if (!a)
                break;
        } else {
            tail->next = b;
            b->prev = tail;
            tail = b;
            b = b->next;
            n_comp++;
            if (!b) {
                b = a;
                break;
            }
        }
    }

    /* Finish linking remainder of list b on to tail */
    tail->next = b;
    do {
        if (unlikely(!++count))
            cmp(priv, b, b);
        b->prev = tail;
        tail = b;
        b = b->next;
    } while (b);

    /* And the final links to make a circular doubly-linked list */
    tail->next = head;
    head->prev = tail;
}

__attribute__((nonnull(2))) void list_sort(void *priv,
                                           struct list_head *head,
                                           bool descend)
{
    if (list_empty(head) || list_is_singular(head))
        return;

    struct list_head *list = head->next, *pending = NULL;
    size_t count = 0; /* Count of pending */

    if (list == head->prev) /* Zero or one elements */
        return;

    /* Convert to a null-terminated singly-linked list. */
    head->prev->next = NULL;

    list_cmp_func_t q_cmp = descend ? q_greater : q_less;

    do {
        size_t bits;
        struct list_head **tail = &pending;

        /* Find the least-significant clear bit in count */
        for (bits = count; bits & 1; bits >>= 1)
            tail = &(*tail)->prev;
        /* Do the indicated merge */
        if (likely(bits)) {
            struct list_head *a = *tail, *b = a->prev;

            a = merge(priv, q_cmp, b, a);
            /* Install the merged result in place of the inputs */
            a->prev = b->prev;
            *tail = a;
        }

        /* Move one element from input list to pending */
        list->prev = pending;
        pending = list;
        list = list->next;
        pending->next = NULL;
        count++;
    } while (list);

    /* End of input; merge together all the pending lists. */
    list = pending;
    pending = pending->prev;
    for (;;) {
        struct list_head *next = pending->prev;

        if (!next)
            break;
        list = merge(priv, q_cmp, pending, list);
        pending = next;
    }
    /* The final merge, rebuilding prev links */
    merge_final(priv, q_cmp, head, pending, list);
}

static void my_merge(struct list_head **head,
                     struct list_head *a,
                     struct list_head *b,
                     list_cmp_func_t cmp)
{
    struct list_head **tail = head;

    while (a && b) {
        if (cmp(NULL, a, b) <= 0) {
            *tail = a;
            a = a->next;
            n_comp++;
        } else {
            *tail = b;
            b = b->next;
            n_comp++;
        }

        tail = &(*tail)->next;
    }

    if (a) {
        *tail = a;
    } else if (b) {
        *tail = b;
    }
}

void my_sort(struct list_head *head, bool descend)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;

    list_cmp_func_t q_cmp = descend ? q_greater : q_less;

    // Split original list into sorted sublists.
    struct list_head *h = head->next, *t = head->next;
    struct list_head *sublist = NULL;
    while (h != head) {
        while (t->next != head && q_cmp(NULL, t, t->next) <= 0) {
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
            my_merge(subhead, currsub, currsub->prev, q_cmp);
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

uintptr_t os_random(uintptr_t seed)
{
    /* ASLR makes the address random */
    uintptr_t x = (uintptr_t) &os_random ^ seed;
#if defined(__APPLE__)
    x ^= (uintptr_t) mach_absolute_time();
#else
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    x ^= (uintptr_t) time.tv_sec;
    x ^= (uintptr_t) time.tv_nsec;
#endif
    /* Do a few randomization steps */
    uintptr_t max = ((x ^ (x >> 17)) & 0x0F) + 1;
    for (uintptr_t i = 0; i < max; i++)
        x = random_shuffle(x);
    assert(x);
    return x;
}

static void fill_rand_string(char *buf, size_t buf_size)
{
    size_t len = 0;
    while (len < MIN_RANDSTR_LEN)
        len = rand() % buf_size;

    randombytes((uint8_t *) buf, len);
    for (size_t n = 0; n < len; n++)
        buf[n] = charset[buf[n] % (sizeof(charset) - 1)];
    buf[len] = '\0';
}

static void q_comp_rand_init(struct list_head *head, int size, int random)
{
    char randstr_buf[MAX_RANDSTR_LEN];
    for (int i = 0; i < size; i++) {
        fill_rand_string(randstr_buf, sizeof(randstr_buf));
        q_insert_tail(head, randstr_buf);
    }
}

static void q_comp_fixed_init(struct list_head *head, int size)
{
    if (!file_path) {
        fprintf(stderr, "File path is not specified.\n");
        return;
    }

    FILE *fp;
    char *buf = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(file_path, "r");

    if (fp == NULL) {
        printf("File opening error. \n");
        exit(EXIT_FAILURE);
    }

    int cnt = 0;
    while (cnt < size && (read = getline(&buf, &len, fp)) != -1) {
        q_insert_tail(head, buf);
        cnt++;
    }

    fclose(fp);
}

static void getPerf(int n, int random, bool descend)
{
    struct list_head *head = q_new();

    if (random == 0) {
        q_comp_fixed_init(head, n);
    } else {
        q_comp_rand_init(head, n, random);
    }

    clock_t start_time = clock();
    if (sort == 1) {
        list_sort(NULL, head, descend);
    } else {
        my_sort(head, descend);
    }
    clock_t end_time = clock();

    exec_time = (double) (end_time - start_time) / CLOCKS_PER_SEC;
    k = log2(n_data) - (double) (n_comp - 1) / n;

    printf("Execution time: %.lf\n", exec_time);
    printf("Total Number of Comparison: %d\n", n_comp);
    printf("Value of K= %.5lf\n", k);

    q_free(head);
}

static inline void show_config()
{
    printf("Experiment Setting: \n");
    printf("The number of tests: %d\n", n_tests);
    printf("The number of queue node: %d\n", n_data);

    printf("Sort Alorithm: ");
    if (sort == 1) {
        printf("list_sort\n");
    } else {
        printf("q_sort\n");
    }

    printf("Fixed/Random Queue Content?: ");
    if (random_data == 1) {
        printf("Random\n");
    } else {
        printf("Fixed\n");
    }
}

int main(int argc, char *argv[])
{
    srand(os_random(getpid() ^ getppid()));

    int c;
    while ((c = getopt(argc, argv, "s:n:r:t:f:")) != -1) {
        switch (c) {
        case 's':
            sort = atoi(optarg);
            break;
        case 'n':
            n_data = atoi(optarg);
            break;
        case 'r': {
            random_data = atoi(optarg);
            break;
        }
        case 't':
            n_tests = atoi(optarg);
            break;
        case 'f':
            file_path = optarg;
            break;
        default:
            printf("Unknown option '%c'\n", c);
            break;
        }
    }

    show_config();
    printf("---------------\n");

    int total_comp = 0;
    double total_exec_time = 0;
    double total_k = 0;
    for (int i = 0; i < n_tests; i++) {
        printf("Test %d\n", i);
        getPerf(n_data, random_data, descend);
        total_comp += n_comp;
        total_exec_time += exec_time;
        total_k += k;
        n_comp = 0;
        exec_time = 0;
        k = 0;
        printf("---------------\n");
    }

    printf("Average Number of Comparison : %.5lf\n",
           (double) total_comp / n_tests);
    printf("Average Execution Time : %.5lf\n", total_exec_time / n_tests);
    printf("Average k : %.5lf\n", total_k / n_tests);

    return 0;
}