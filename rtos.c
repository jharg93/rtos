#include <stdio.h>
#include <ucontext.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>

typedef struct task task_t;

#define MAXTASK 32

enum { RUNNABLE, SLEEPING };

struct task {
  void *sleep;
  ucontext_t state;
};

int curr, ntsk;
task_t *tasks[MAXTASK];

void schedule() {
  int prev;

  prev = curr;
  do {
    curr = (curr + 1) % ntsk;
  } while (curr != prev && tasks[curr]->sleep);
  printf("%d -> %d\n", prev, curr);
  swapcontext(&tasks[prev]->state, &tasks[curr]->state);
}

/*================================================*
 * Task code
 *================================================*/
void task_init(task_t *t, void *sp, int splen, void (*fn)(void *), void *arg) {
  if (!sp && splen) {
    sp = malloc(splen);
    memset(sp, 0, splen);
  }
  getcontext(&t->state);
  t->state.uc_stack.ss_sp = sp;
  t->state.uc_stack.ss_size = splen;
  makecontext(&t->state, (void (*)())fn, 1, arg);
  tasks[ntsk++] = t;
}

/*================================================*
 * Event code
 *================================================*/
struct evt {
  int key;
};

void evt_init(struct evt *evt) {
  evt->key = 0;
}

void evt_wait(struct evt *evt) {
  int ck = evt->key;

  while (ck == evt->key) {
    tasks[curr]->sleep = evt;
    schedule();
  }
}

void evt_signal(struct evt *evt) {
  int i;
  
  evt->key++;
  for (i = 0; i < ntsk; i++) {
    if (tasks[i]->sleep == evt)
      tasks[i]->sleep = NULL;
  }
  schedule();
}
  
/*================================================*
 * Semaphore code
 *================================================*/
struct sem {
  struct evt evt;
  int val;
  const char *name;
};

void sem_init(struct sem *s, int ic, const char *name) {
  evt_init(&s->evt);
  s->val = ic;
  s->name = strdup(name);
}

void sem_wait(struct sem *s) {
  while (s->val == 0) {
    evt_wait(&s->evt);
  }
  s->val--;
}

void sem_post(struct sem *s) {
  s->val++;
  evt_signal(&s->evt);
}

struct evt e = { 0 };
  
void i2ctask(void *arg)
{
  for(;;) {
    printf("i2c wait %ld\n", (uintptr_t)arg);
    evt_wait(&e);
    printf("i2c do the thing %d\n", (uintptr_t)arg);
  }
}

void runtask(void *arg) {
  for(;;) {
    printf("runtask: %d\n", (uintptr_t)arg);
    if ((rand() % 10) == 0)
      evt_signal(&e);
  }
}

struct sem full, empty, mutex;

#define BSIZE 10
int buf[BSIZE], bi, bo;

void prod_task(void *arg)
{
  for(;;) {
    if ((rand() % 10) == 0) {
      sem_wait(&empty);
      sem_wait(&mutex);
      buf[bi % BSIZE] = bi;
      printf("produce one %d\n", buf[bi % BSIZE]);
      bi = (bi + 1);
      sem_post(&mutex);
      sem_post(&full);
    }
    else {
      schedule();
    }
  }
}

void cons_task(void *arg)
{
  for(;;) {
    if ((rand() % 20) == 0) {
      sem_wait(&full);
      sem_wait(&mutex);
      printf("@@ consume one %d\n", buf[bo]);
      bo = (bo + 1) % BSIZE;
      sem_post(&mutex);
      sem_post(&empty);
    }
    else {
      schedule();
    }
  }
}

void idle_task()
{
  task_t self;

  swapcontext(&self.state, &tasks[0]->state);
}

task_t rt;
task_t et;
task_t ct;
task_t pt;

int
main (void)
{
  task_t init, *t;
  int i, stksz;

  stksz = 8192;
  sem_init(&mutex, 1, "mutex");
  sem_init(&empty, BSIZE, "empty");
  sem_init(&full, 0, "full");
  
  //task_init(&rt, NULL, stksz, runtask, (void *)99);
  //task_init(&et, NULL, stksz, i2ctask, (void *)100);
  task_init(&pt, NULL, stksz, prod_task, (void *)120);
  task_init(&ct, NULL, stksz, cons_task, (void *)130);
#if 0
  for (i = 0 ; i < 32; i++) {
    stksz = 8192;
    t = malloc(sizeof(task_t) + stksz);
    memset(t, 0, sizeof(task_t) + stksz);
    task_init(t, &t[1], stksz, thetask, (void *)i);
  }
#endif
  idle_task();
  //swapcontext(&init, &tasks[0]->state);
  printf("Exit....\n");
  return 0;
}
