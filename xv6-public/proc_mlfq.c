#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

typedef struct {
  int next;
  int prev;
  int level;
  struct proc* target_p;
} QNode;

typedef enum {
  IDLE = 0,
  LOCKED
} MLFQState;

typedef struct {
  MLFQState state;                 // if scheduler locked(LOCKED) or not(IDLE)
  struct proc* head[NMLFQLEVEL];   // NULL when queue is empty (queue has no head)
  int q_enter_id[NMLFQLEVEL];      // increase 1 when new process enter, 0 is default
  int locked_proc_idx;             // process that has called schedulerLock()
  struct proc* ptable_ptr;         // implement queue as linked list
} MLFQ;

MLFQ _mlfq;

void
init_mlfq(MLFQ* mlfq) {
  int i;
  struct proc* qdata_ptr;
  
  mlfq->state = IDLE;

  for (i = 0; i < NMLFQLEVEL; ++i) {
    mlfq->head[i] = NULL;    // NULL when empty
  }

  for (qdata_ptr = mlfq->q_data; qdata_ptr < &(mlfq->q_data[NPROC]); qdata_ptr++) {
    qdata_ptr->next = NULL;  // NULL when prev is head
    qdata_ptr->prev = NULL;  // NULL when has no next
    qdata_ptr->level = NULL; // NULL when has no head
  }
}

// return 0  : same pid
// return 1  : lhs has smaller pid
// return -1 : rhs has smaller pid
int compare_pid(struct proc* lhs, struct proc* rhs) {
  if (lhs->pid == rhs->pid) {
    return 0;
  } else if (lhs->pid < rhs->pid) {
    return 1;
  } else {
    return -1;
  }
}

// Case 1: L0, L1 queue
// Just compare pid for FCFS scheduling. 
// Small pid has higher priority
//
// Case 2: L2 queue
// Needs to implement Priority scheduling.
// First compare priority value in proc.
// Small priority value has higher priority.
// If both have same priority, then compare pid.
//
// return 0  : same priority
// return 1  : lhs has higher priority
// return -1 : rhs has higher priority
int
compare_priority(int level, struct proc* lhs, struct proc* rhs) {
  int result;
  
  if (level == 0 || level == 1) {
    result = compare_pid(lhs, rhs);

    return result;
  }

  if (level == 2) {
    if (lhs->priority < rhs->priority) {
      return 1;
    } else if (lhs->priority > rhs->priority) {
      return -1;
    } else {
      result = compare_pid(lhs, rhs);

      return result;
    }
  }

  // level is not one of 0~2
  cprintf("level: %d", level);
  panic("strange level!");
}

void 
insert_queue(MLFQ* mlfq, int idx, struct proc* p, int level) {
  int head = mlfq->head[level];
  QNode* qdata_ptr = mlfq->q_data;
  QNode* target_ptr;

  int found = 0;
  int temp;
  
  p->priority = MLFQMAXPRIORIY;
  p->level = level;

  qdata_ptr[idx].level = level;
  qdata_ptr[idx].target_p = p;

  // if queue has no head, set target as head
  if (head == -1) {
    mlfq->head[level] = idx;
    qdata_ptr[idx].prev = -1;
    qdata_ptr[idx].next = -1;
  
    return;
  }

  // iterate queue(linked list)
  for (target_ptr = qdata_ptr + head; !found;) {
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