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
  LOCKED,
  UNLOCK_REQUIRE,
  LOCKED_SLEEPING
} MLFQState;

typedef struct _QList {
  struct proc* head;               // NULL when queue is empty
  struct proc* tail;               // NULL when queue is empty
} QList;

typedef struct _MLFQ {
  const int MAX_TIME_QUANTUM[NMLFQLEVEL];
  int global_tick;

  MLFQState state;                 // if scheduler locked(LOCKED) or not(IDLE)
  struct proc* ptable_ptr;         // implement queue as linked list

  QList sched_queue[NMLFQLEVEL];   // Linked list for L0~L2 queue
  int l2q_enter_id;                // increase 1 when new process enter, 0 is default

  struct proc* locked_proc;        // process that has called schedulerLock()
} MLFQ;

MLFQ _mlfq = {.MAX_TIME_QUANTUM = {MLFQL0TIMEQ, MLFQL1TIMEQ, MLFQL2TIMEQ}};

void
print_mlfq_err(MLFQ* mlfq, struct proc* p) {
  cprintf(
    "pid: %d, used time quantum: %d, level: %d\n", 
    p->pid, 
    mlfq->MAX_TIME_QUANTUM[p->mlfq_info.level] - p->mlfq_info.tick_left, 
    p->mlfq_info.level
  );
}

void
print_p_info(struct proc* p) {
  cprintf(
    "pid: %d, time quantum left: %d, level: %d\n", 
    p->pid, 
    p->mlfq_info.tick_left, 
    p->mlfq_info.level
  );
}

void
init_mlfq(MLFQ* mlfq, struct proc* ptable_procs) {
  int i;
  struct proc* iter_ptr;
  
  mlfq->state = IDLE;
  mlfq->ptable_ptr = ptable_procs;
  mlfq->l2q_enter_id = 0;
  mlfq->locked_proc = NULL;

  for (i = 0; i < NMLFQLEVEL; ++i) {
    mlfq->sched_queue[i].head = NULL;    // NULL when empty
    mlfq->sched_queue[i].tail = NULL;    // NULL when empty
  }

  for (iter_ptr = mlfq->ptable_ptr; iter_ptr < &((mlfq->ptable_ptr)[NPROC]); iter_ptr++) {
    iter_ptr->mlfq_info.prev = NULL;  // NULL when no prev (I'm the head element)
    iter_ptr->mlfq_info.next = NULL;  // NULL when no next (I'm the tail element)
    iter_ptr->mlfq_info.level = -1;   // -1 when has no head
  }
}

// return 0  : same priority value
// return 1  : lhs has smaller priority value
// return -1 : rhs has smaller priority value
int 
compare_pvalue(int lhs, int rhs) {
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

    if (compare == 0) {
      print_p_info(lhs);
      print_p_info(rhs);
      panic("enter_id cannot be same");
    }

    return compare;
  }
}

void
push_first_elem(QList* queue, struct proc* p) {
  queue->head = p;
  queue->tail = p;

  p->mlfq_info.prev = NULL;
  p->mlfq_info.next = NULL;

  return;
}

void
push_head(QList* queue, struct proc* p) {
  struct proc* temp;

  if (queue->head == NULL) {
    push_first_elem(queue, p);

    return;
  }
  
  temp = queue->head;
  queue->head = p;

  temp->mlfq_info.prev = p;

  p->mlfq_info.next = temp;
  p->mlfq_info.prev = NULL;
}

void push_tail(QList* queue, struct proc* p) {
  struct proc* temp;

  if (queue->head == NULL) {
    push_first_elem(queue, p);

    return;
  }

  temp = queue->tail;
  queue->tail = p;
  
  temp->mlfq_info.next = p;

  p->mlfq_info.prev = temp;
  p->mlfq_info.next = NULL;
}

void
push_by_priority(QList* queue, struct proc* p) {
  int found = FALSE;

  struct proc* iter_ptr;
  struct proc* temp;

  if (queue->head == NULL) {
    push_first_elem(queue, p);
    
    return;
  }

  // iterate from head
  for (iter_ptr = queue->head; iter_ptr != NULL || !found; iter_ptr = iter_ptr->mlfq_info.next) {
    if (compare_priority(p, iter_ptr) <= 0) {
      // insert before target
      temp = iter_ptr->mlfq_info.prev;
      iter_ptr->mlfq_info.prev = p;
      p->mlfq_info.next = iter_ptr;
      p->mlfq_info.prev = temp;

      if (temp == NULL) {
        // target is head
        queue->head = p;
      } else {
        temp->mlfq_info.next = p;
      }

      found = TRUE;
    }
  }
  
  if (!found) {
    // insert to tail
    push_tail(queue, p);
  }
}

struct proc*
pop_tail(QList* queue) {
  struct proc** tail_ptr = &(queue->tail);
  struct proc* tail_proc;

  tail_proc = *(tail_ptr);
  *(tail_ptr) = tail_proc->mlfq_info.prev;

  if (tail_proc->mlfq_info.prev == NULL) { // if tail was head
    queue->head = NULL;
    return tail_proc;
  }

  tail_proc->mlfq_info.prev->mlfq_info.next = NULL;
  tail_proc->mlfq_info.prev = NULL;
  return tail_proc;
}

// insert process in queue.
// initialize tick 
void 
insert_queue(MLFQ* mlfq, struct proc* p, int level, int set_priority, int set_timequantum) {
  struct proc* head = mlfq->sched_queue[level].head;

  struct proc* target_ptr;
  struct proc* temp;
  int found = 0;

  struct proc** head_ptr = &(mlfq->sched_queue[level].head);
  struct proc** tail_ptr = &(mlfq->sched_queue[level].tail);

  // common setting for all queues
  if (set_priority) {
    p->mlfq_info.priority.pvalue = MLFQMAXPRIORIY;
  }
  if (set_timequantum) {
    p->mlfq_info.tick_left = mlfq->MAX_TIME_QUANTUM[level];
  }
  p->mlfq_info.level = level;

  if (level == L0 || level == L1) {
    // just put the process at head
    push_head(&(mlfq->sched_queue[level]), p);
  } else {
    // iterate from head
    p->mlfq_info.priority.enter_id = (mlfq->l2q_enter_id)++;
    push_by_priority(&(mlfq->sched_queue[level]), p);
  }
}

// mlfq: target mlfq pointer
// p: target process
// keep_level: true(!= 0) or false(0). keep level value when true. if not, set it -1
void
delete_from_queue(MLFQ* mlfq, struct proc* p, int keep_level) {
  int level = p->mlfq_info.level;

  struct proc** head_ptr = &(mlfq->sched_queue[level].head);
  struct proc** tail_ptr = &(mlfq->sched_queue[level].tail);

  if (!keep_level) {
    p->mlfq_info.level = -1;
    p->mlfq_info.tick_left = 0;
  }

  if (p->mlfq_info.next == NULL) { // if p is tail
    *(tail_ptr) = p->mlfq_info.prev;
  } else {
    p->mlfq_info.next->mlfq_info.prev = p->mlfq_info.prev;
  }

  if (p->mlfq_info.prev == NULL) { // if p is head
    *(head_ptr) = p->mlfq_info.next;
  } else {
    p->mlfq_info.prev->mlfq_info.next = p->mlfq_info.next;
  }

  p->mlfq_info.prev = NULL;
  p->mlfq_info.next = NULL;
}

// return the level of queue which can be scheduled next.
// check from L0 first, and L1, L2 sequentially.
// panic when there's no queue can be scheduled.
int
get_able_queue(MLFQ* mlfq) {
  int i;
  
  for (i = 0; i < NMLFQLEVEL; i++) {
    if (mlfq->sched_queue[i].head != NULL) {
      return i;
    }
  }

  panic("No queue can be scheduled");
}

struct proc*
mlfq_select_target(MLFQ* mlfq) {
  int target_level;
  struct proc* target_proc;

  if (mlfq->state == UNLOCK_REQUIRE) {
    panic("mlfq state is UNLOCK_REQUIRE");
  }

  if (mlfq->state == LOCKED) {
    return mlfq->locked_proc;
  }

  target_level = get_able_queue(mlfq);
  target_proc = pop_tail(&(mlfq->sched_queue[target_level]));

  if (target_proc->state != RUNNABLE) {
    print_mlfq_err(mlfq, target_proc);
    cprintf("Schedule target proc state was: %d", (int)target_proc->state);
    panic("Schedule target process was not runnable");
  }

  return target_proc;
}

// if 
void
back_to_mlfq(MLFQ* mlfq, struct proc* p) {
  if (mlfq->state == LOCKED) {
    return;
  }
  
  if (mlfq->state == UNLOCK_REQUIRE) {
    push_tail(&(mlfq->sched_queue[L0]), p);
    mlfq->state = IDLE;
    return;
  }

  if (p->state == RUNNABLE) {
    if (p->mlfq_info.tick_left == 0) {
      print_mlfq_err(mlfq, p);
      panic("Process already used all time quantum!");
    }
    
    (p->mlfq_info.tick_left)--;
    
    if (p->mlfq_info.tick_left == 0) { // Used all time quantum, move to lower queue
      if (p->mlfq_info.level == L0 || p->mlfq_info.level == L1) {
        (p->mlfq_info.level)++;
        insert_queue(mlfq, p, p->mlfq_info.level, FALSE, TRUE);
      } else if (p->mlfq_info.level == L2) {
        if (p->mlfq_info.priority.pvalue > 0) {
          (p->mlfq_info.priority.pvalue)--;
        }
        // L2 to L2 so just set time quantum and push to L2 queue
        p->mlfq_info.tick_left = mlfq->MAX_TIME_QUANTUM[L2];
        push_by_priority(&(mlfq->sched_queue[L2]), p); 
      } else {
        print_mlfq_err(mlfq, p);
        panic("Process has wrong level!");
      }
    } else { // time quantum has left, push into same level queue
      if (p->mlfq_info.level == L0 || p->mlfq_info.level == L1) {
        push_head(&(mlfq->sched_queue[p->mlfq_info.level]), p);
      } else if (p->mlfq_info.level == L2) {
        push_by_priority(&(mlfq->sched_queue[L2]), p);
      } else {
        print_mlfq_err(mlfq, p);
        panic("Process has wrong level!");
      }
    }

    return;
  }

  print_mlfq_err(mlfq, p);
  cprintf("state: %d", p->pid, p->state);
  panic("Process is not RUNNABLE");
}

void
relocate_by_priority(MLFQ* mlfq, int pid, int priority) {
  struct proc* iter_ptr;
  struct proc* target_proc = NULL;
  
  for (iter_ptr = mlfq->ptable_ptr; iter_ptr < &((mlfq->ptable_ptr)[NPROC]); iter_ptr++) {
    if (iter_ptr->pid == pid) {
      target_proc = iter_ptr;
      break;
    }
  }

  if (target_proc == NULL) {
    cprintf("pid: %d", pid);
    panic("couldn't find the pid");
  }

  if (target_proc->state != RUNNABLE && target_proc->state != SLEEPING) {
    print_mlfq_err(mlfq, target_proc);
    panic("target proc not RUNNABLE or SLEEPING so couldn't relocate");
  }

  target_proc->mlfq_info.priority.pvalue = priority;

  if (target_proc == mlfq->locked_proc && mlfq->state == LOCKED_SLEEPING) {
    return;
  }

  if (target_proc->mlfq_info.level == L2 && target_proc->state == RUNNABLE) {
    delete_from_queue(mlfq, target_proc, TRUE);
    push_by_priority(&(mlfq->sched_queue[L2]), target_proc);
  } 
}

void
prirority_boost(MLFQ* mlfq) {
  struct proc* target_proc;
  struct proc* iter_ptr;

  QList* L1Q_ptr = &(mlfq->sched_queue[L1]);
  QList* L2Q_ptr = &(mlfq->sched_queue[L2]);

  if (mlfq->state == LOCKED) {
    mlfq->state = IDLE;
    
    target_proc = mlfq->locked_proc;
    mlfq->locked_proc = NULL;

    target_proc->mlfq_info.level = L0;

    push_tail(&(mlfq->sched_queue[L0]), target_proc);
  }

  while (L1Q_ptr->head != NULL) {
    target_proc = pop_tail(L1Q_ptr);
    insert_queue(mlfq, target_proc, L0, TRUE, TRUE);
  }

  while (L2Q_ptr->head != NULL) {
    target_proc = pop_tail(L2Q_ptr);
    insert_queue(mlfq, target_proc, L0, TRUE, TRUE);
  }

  for (iter_ptr = mlfq->ptable_ptr; iter_ptr < &((mlfq->ptable_ptr)[NPROC]); iter_ptr++) {
    if (iter_ptr->state == RUNNABLE) {
      iter_ptr->mlfq_info.priority.pvalue = MLFQMAXPRIORIY;
      iter_ptr->mlfq_info.tick_left = mlfq->MAX_TIME_QUANTUM[L0];
    }
  }
}

void 
boost_check(MLFQ* mlfq) {
  mlfq->global_tick++;

  if (mlfq->global_tick >= 100) {
    mlfq->global_tick = 0;
    prirority_boost(mlfq);
  }
}

//TODO: exit, sleep 할때도 자동 unlock 해줘야함
void
scheduler_lock(MLFQ* mlfq, struct proc* target_proc) {
  if (mlfq->state == LOCKED) {
    print_mlfq_err(mlfq, target_proc);
    panic("mlfq is already locked");
  }

  mlfq->state = LOCKED;
  mlfq->locked_proc = target_proc;
}

void
scheduler_unlock(MLFQ* mlfq) {
  struct proc* target_proc;

  if (mlfq->state != LOCKED) {
    panic("mlfq is not locked");
  }
  if (mlfq->locked_proc == NULL) {
    panic("no process locked");
  }

  mlfq->state = UNLOCK_REQUIRE;

  target_proc = mlfq->locked_proc;
  mlfq->locked_proc = NULL;

  target_proc->mlfq_info.priority.pvalue = MLFQMAXPRIORIY;
  target_proc->mlfq_info.tick_left = mlfq->MAX_TIME_QUANTUM[L0];
  target_proc->mlfq_info.level = L0;
}

void
check_lock_state_when_sched(MLFQ* mlfq, struct proc* p) {
  if (mlfq->state == LOCKED) {
    if (p->state == SLEEPING) {
      mlfq->state = LOCKED_SLEEPING;
      return;
    }

    if (p->state == ZOMBIE) {
      scheduler_unlock(mlfq);
      return;
    }
  }
}

void
check_wakeup(MLFQ* mlfq, struct proc* p) {
  if (mlfq->state == LOCKED_SLEEPING) {
    if (p == mlfq->locked_proc) {
      mlfq->state = LOCKED;
      return;
    }
  }

  insert_queue(mlfq, p, p->mlfq_info.level, FALSE, TRUE);
}