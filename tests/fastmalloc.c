/*
 * Test that a small allocation taken from the thread-local free list
 * leaves no pointer to the next free object on the stack.  A parked Crystal fiber
 * keeps such a stale word for good, and with it whatever is allocated
 * into that object later.
 * The test reads below the stack pointer and needs to know how the
 * collector is built, so it is built only with a static libgc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "private/gc_priv.h"

#if !defined(THREAD_LOCAL_ALLOC) || defined(GC_ASSERTIONS) \
    || defined(STACK_GROWS_UP)

int
main(void)
{
  printf("Skipping, no bare thread-local allocation\n");
  return 77;
}

#else

#define OBJ_SIZE (2 * sizeof(void *))
#define N_WORDS 2048
#define N_ROUNDS 100

static word stack_words[N_WORDS];

static GC_ATTR_NOINLINE void *
alloc_one(void)
{
  void *p = GC_MALLOC(OBJ_SIZE);

  if (NULL == p) {
    fprintf(stderr, "Out of memory\n");
    exit(69);
  }
  return p;
}

static GC_ATTR_NOINLINE void
clear_stack(void)
{
  volatile char buf[(N_WORDS + 512) * sizeof(word)];

  memset((void *)buf, 0, sizeof(buf));
}

/* Copy the stack words below those of main, where its callees ran.    */
static GC_ATTR_NOINLINE void
save_stack(const volatile word *top)
{
  size_t i;

  for (i = 0; i < N_WORDS; i++)
    stack_words[i] = top[-(signed_word)i - 1];
}

int
main(void)
{
  volatile word top = 0;
  size_t i, round, stale = 0;

  GC_INIT();
  if (GC_get_find_leak()) {
    printf("Skipping in leak detection mode\n");
    return 77;
  }
  /* Keep the free lists as they are between allocations.     */
  GC_disable();
  /* Get past the objects allocated directly.                 */
  for (i = 0; i < 1000; i++)
    (void)alloc_one();

  for (round = 0; round < N_ROUNDS; round++) {
    word next;

    clear_stack();
    (void)alloc_one();
    save_stack(&top);
    next = (word)alloc_one();
    for (i = 0; i < N_WORDS; i++) {
      if (stack_words[i] == next) {
        stale++;
        break;
      }
    }
  }
  /* An allocation that refills the free list still leaves one.       */
  if (stale > N_ROUNDS / 10) {
    fprintf(stderr, "%lu of %d allocations left a pointer to the next"
                    " free object on the stack\n",
            (unsigned long)stale, N_ROUNDS);
    return 1;
  }
  printf("SUCCEEDED\n");
  return 0;
}

#endif
