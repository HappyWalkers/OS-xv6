#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

// #define SOL

static int nthread = 1;
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  int nthread;      // Number of threads that have reached this round of the barrier
  int round;     // Barrier round
} bstate;

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
}

static void 
barrier()
{
  if(pthread_mutex_lock(&bstate.barrier_mutex) == 0) {
    // every thread that reaches the barrier increments nthread
    bstate.nthread++;

    // if nthread is less than nthread, the thread waits for the condition variable
    // when nthread reaches nthread, the last thread reaching the barrier resets nthread to, increments round, and broadcasts the condition variable
    if(bstate.nthread < nthread) {
      pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    } else {
      bstate.nthread = 0;
      bstate.round++;
      pthread_cond_broadcast(&bstate.barrier_cond);
    }

    pthread_mutex_unlock(&bstate.barrier_mutex);
  }
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    // printf("thread %ld is at round %d\n", n, t);
    // printf("the expected round is %d\n", i);
    assert (i == t);
    barrier();
    usleep(random() % 100);
  }
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
