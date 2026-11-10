/*
 * Test that the finalizer and disappearing link hash tables free the
 * table they outgrow.  It is otherwise kept by any stale pointer to it,
 * and its buckets keep whatever reuses the entries freed since.
 */

#include <stdio.h>
#include <stdlib.h>

#include "gc.h"

#define N_ENTRIES 4096

static void GC_CALLBACK
finalizer(void *obj, void *client_data)
{
  (void)obj;
  (void)client_data;
}

static void
check_freed(const char *table, size_t before)
{
  size_t freed = GC_get_expl_freed_bytes_since_gc() - before;

  /* The last table outgrown has half a bucket per entry.       */
  if (freed < N_ENTRIES / 2 * sizeof(void *)) {
    fprintf(stderr, "Outgrown %s tables not freed (%lu bytes freed)\n",
            table, (unsigned long)freed);
    exit(1);
  }
}

int
main(void)
{
  void **links;
  size_t before, i;

  GC_INIT();
  /* No collection, so the freed bytes counter is not reset.    */
  GC_disable();
  links = (void **)GC_MALLOC(N_ENTRIES * sizeof(void *));
  if (NULL == links) {
    fprintf(stderr, "Out of memory\n");
    exit(69);
  }

  before = GC_get_expl_freed_bytes_since_gc();
  for (i = 0; i < N_ENTRIES; i++) {
    links[i] = GC_MALLOC_ATOMIC(sizeof(size_t));
    if (NULL == links[i]) {
      fprintf(stderr, "Out of memory\n");
      exit(69);
    }
    GC_REGISTER_FINALIZER_NO_ORDER(links[i], finalizer, NULL, NULL, NULL);
  }
  check_freed("finalizer", before);

  before = GC_get_expl_freed_bytes_since_gc();
  for (i = 0; i < N_ENTRIES; i++) {
    if (GC_GENERAL_REGISTER_DISAPPEARING_LINK(&links[i], links[i])
        == GC_NO_MEMORY) {
      fprintf(stderr, "Out of memory\n");
      exit(69);
    }
  }
  check_freed("disappearing link", before);

  printf("SUCCEEDED\n");
  return 0;
}
