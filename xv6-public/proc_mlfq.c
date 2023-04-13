#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

typedef enum {
  IDLE = 0,
  LOCKED
} MLFQState;

typedef struct {
  struct proc* head;               // NULL when queue is empty
  struct proc* tail;               // NULL when queue is empty
} QList;

typedef struct {
  const int MAX_TIME_QUANTUM[NMLFQLEVEL];

  MLFQState state;                 // if scheduler locked(LOCKED) or not(IDLE)
  struct proc* ptable_ptr;         // implement queue as linked list

  QList sched_queue[NMLFQLEVEL];   // Linked list for L0~L2 queue
  QList sleeping_queue;            // linked list for sleeping procs
  int l2q_enter_id;                // increase 1 when new process enter, 0 is default
  struct proc* locked_proc;        // process that has called schedulerLock()
} MLFQ;

MLFQ _mlfq = {.MAX_TIME_QUANTUM = {MLFQL0TIMEQ, MLFQL1TIMEQ, MLFQL2TIMEQ}};

void
init_mlfq(MLFQ* mlfq, struct proc* ptable_procs) {
  int i;
  struct proc* proc_ptr;
  
  mlfq->state = IDLE;
  mlfq->ptable_ptr = ptable_procs;
  mlfq->l2q_enter_id = 0;
  mlfq->locked_proc = NULL;

  mlfq->sleeping_queue.head = NULL;
  mlfq->sleeping_queue.tail = NULL;

  for (i = 0; i < NMLFQLEVEL; ++i) {
    mlfq->sched_queue[i].head = NULL;    // NULL when empty
    mlfq->sched_queue[i].tail = NULL;    // NULL when empty
  }

  for (proc_ptr = mlfq->ptable_ptr; proc_ptr < &((mlfq->ptable_ptr)[NPROC]); proc_ptr++) {
    proc_ptr->mlfq_info.prev = NULL;  // NULL when no prev (I'm the head element)
    proc_ptr->mlfq_info.next = NULL;  // NULL when no next (I'm the tail element)
    proc_ptr->mlfq_info.level = -1;   // -1 when has no head
  }
}

// return 0  : same priority value
// return 1  : lhs has smaller priority value
// return -1 : rhs has smaller priority value
int compare_pvalue(int lhs, int rhs) {
  if (lhs == rhs) {
    return 0;
  } else if (lhs < rhs) {
    return 1;
  } else {
    return -1;
  }
}

// for L2 queue
// Needs to implement Priority scheduling.
// First compare priority value in proc.
// Small priority value has higher priority.
// If both have same priority, then compare enter_id.
//
// return 0  : same priority
// return 1  : lhs has higher priority
// return -1 : rhs has higher priority
int
compare_priority(struct proc* lhs, struct proc* rhs) {
  int compare = compare_pvalue(
    lhs->mlfq_info.priority.pvalue, 
    rhs->mlfq_info.priority.pvalue
  );

  if (compare > 0) {
    return 1;
  } else if (compare < 0) {
    return -1;
  } else {
    compare = compare_pvalue(
      lhs->mlfq_info.priority.enter_id, 
      rhs->mlfq_info.priority.enter_id
    );

    return compare;
  }
}

void
push_head(QList* queue, struct proc* p) {
  struct proc** head_ptr = &(queue->head);
  struct proc* temp;
  
  temp = *(head_ptr);
  *(head_ptr) = p;

  p->mlfq_info.next = temp;
  p->mlfq_info.prev = NULL;
  
  temp->mlfq_info.prev = p;
}

struct proc*
pop_tail(QList* queue) {
  struct proc** tail_ptr = &(queue->tail);
  struct proc* temp;

  temp = *(tail_ptr);
  *(tail_ptr) = temp->mlfq_info.prev;

  if (temp->mlfq_info.prev == NULL) {
    queue->head = NULL;
    return temp;
  }

  temp->mlfq_info.prev->mlfq_info.next = NULL;
  temp->mlfq_info.prev = NULL;
  return temp;
}

void 
insert_queue(MLFQ* mlfq, struct proc* p, int level) {
  struct proc* head = mlfq->sched_queue[level].head;

  struct proc* target_ptr;
  struct proc* temp;
  int found = 0;

  struct proc** head_ptr = &(mlfq->sched_queue[level].head);
  struct proc** tail_ptr = &(mlfq->sched_queue[level].tail);

  // common setting for all queues
  p->mlfq_info.priority.pvalue = MLFQMAXPRIORIY;
  p->mlfq_info.level = level;
  p->mlfq_info.tick_left = mlfq->MAX_TIME_QUANTUM[level];

  // if queue has no head, set target as head and tail
  if (head == NULL) {
    *(head_ptr) = p;
    *(tail_ptr) = p;

    p->mlfq_info.prev = NULL;
    p->mlfq_info.next = NULL;

    if (level == L2) {
      p->mlfq_info.priority.enter_id = (mlfq->l2q_enter_id)++;
    }

    return;
  }

  if (level == L0 || level == L1) {
    temp = *(head_ptr);
    *(head_ptr) = p;

    p->mlfq_info.next = temp;
    p->mlfq_info.prev = NULL;

    temp->mlfq_info.prev = p;
  }

  // iterate queue(linked list)
  for (target_ptr = head; !found;) {
    if (compare_priority(level, p, target_ptr->target_p) >= 0) { // priority of p is higher than iter target
      qdata_ptr[idx].prev = target_ptr->prev;
      qdata_ptr[idx].next = target_ptr - qdata_ptr;
      
      target_ptr->prev = idx;
      
      if (target_ptr->prev == -1) { // if prev is head
        mlfq->head[level] = idx;
      } 
    }
  }
}