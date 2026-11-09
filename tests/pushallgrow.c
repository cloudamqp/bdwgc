/*
 * Test that GC_push_all_grow pushes more ranges than the global mark stack
 * holds by growing it, the way Crystal pushes the stack of every parked
 * fiber, and that it pushes eagerly without parallel marking or with mark
 * stack growth switched off.  Every object must stay reachable either way.
 * The test reads GC_mark_stack_size, so it needs a static libgc.
 */

#include <stdio.h>
#include <stdlib.h>

#include "private/gc_priv.h"
#include "gc_mark.h"

#define N_ROOTS 20000

static void **roots;
static GC_push_other_roots_proc prev_push_other_roots;

static void GC_CALLBACK
push_roots(void)
{
  size_t i;

  if (prev_push_other_roots != 0)
    prev_push_other_roots();
  for (i = 0; i < N_ROOTS; i++)
    GC_push_all_grow(&roots[i], &roots[i] + 1);
}

/*
 * Collect a few times, allocating in between to reuse anything freed by
 * a collection that missed a root, exit if any object is lost, and return
 * the size of the global mark stack.
 */
static size_t
collect(void)
{
  size_t i;

  for (i = 0; i < 4; i++) {
    GC_gcollect();
    if (NULL == GC_MALLOC_ATOMIC(N_ROOTS * sizeof(size_t))) {
      fprintf(stderr, "Out of memory\n");
      exit(69);
    }
  }
  for (i = 0; i < N_ROOTS; i++) {
    if (*(size_t *)roots[i] != i) {
      fprintf(stderr, "Object %lu is lost\n", (unsigned long)i);
      exit(1);
    }
  }
  return GC_mark_stack_size;
}

int
main(void)
{
  size_t initial, grown, i;

  GC_set_markers_count(2);
  GC_INIT();
  GC_start_mark_threads();
  initial = GC_mark_stack_size;
  roots = (void **)calloc(N_ROOTS, sizeof(void *));
  if (NULL == roots) {
    fprintf(stderr, "Out of memory\n");
    return 69;
  }
  for (i = 0; i < N_ROOTS; i++) {
    size_t *p = (size_t *)GC_MALLOC_ATOMIC(sizeof(size_t));

    if (NULL == p) {
      fprintf(stderr, "Out of memory\n");
      return 69;
    }
    *p = i;
    roots[i] = p;
  }
  prev_push_other_roots = GC_get_push_other_roots();
  GC_set_push_other_roots(push_roots);

  GC_set_grow_mark_stack(0);
  if (collect() != initial) {
    fprintf(stderr, "Mark stack grew to %lu entries while switched off\n",
            (unsigned long)GC_mark_stack_size);
    return 1;
  }
  GC_set_grow_mark_stack(1);
  grown = collect();
  if (!GC_get_parallel()) {
    if (grown != initial) {
      fprintf(stderr, "Mark stack grew to %lu entries with one marker\n",
              (unsigned long)grown);
      return 1;
    }
    printf("Pushed eagerly without parallel marking\n");
  } else if (grown < N_ROOTS) {
    fprintf(stderr, "Mark stack did not grow\n");
    return 1;
  } else {
    printf("Mark stack grew from %lu to %lu entries\n",
           (unsigned long)initial, (unsigned long)grown);
  }
  printf("SUCCEEDED\n");
  return 0;
}
