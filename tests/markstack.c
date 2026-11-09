/*
 * Test that the global mark stack grows when the roots pushed by the
 * collecting thread overflow it with parallel marking, unless
 * GC_set_grow_mark_stack(0) is called.  An overflow that never grows the
 * mark stack makes every collection fall back to a serial heap rescan.
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
  if (prev_push_other_roots != 0)
    prev_push_other_roots();
  GC_push_all_eager(roots, roots + N_ROOTS);
}

/*
 * Collect a few times, exit if any object is lost, and return the size of
 * the global mark stack.
 */
static size_t
collect(void)
{
  size_t i;

  for (i = 0; i < 4; i++)
    GC_gcollect();
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
  if (!GC_get_parallel()) {
    printf("Skipping the test without parallel marking\n");
    return 77;
  }
  if (GC_get_grow_mark_stack()
      != (getenv("GC_DISABLE_GROW_MARK_STACK") == NULL)) {
    fprintf(stderr, "Mark stack growth is not on by default\n");
    return 1;
  }
  initial = GC_mark_stack_size;
  roots = (void **)calloc(N_ROOTS, sizeof(void *));
  if (NULL == roots) {
    fprintf(stderr, "Out of memory\n");
    return 69;
  }
  for (i = 0; i < N_ROOTS; i++) {
    size_t *p = (size_t *)GC_MALLOC(sizeof(size_t));

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
  if (grown <= initial) {
    fprintf(stderr, "Mark stack did not grow\n");
    return 1;
  }
  printf("Mark stack grew from %lu to %lu entries\n", (unsigned long)initial,
         (unsigned long)grown);
  printf("SUCCEEDED\n");
  return 0;
}
