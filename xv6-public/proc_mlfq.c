#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "proc_mlfq.h"
#include "spinlock.h"

void
print_mlfq_err(MLFQ* mlfq, struct proc* p) {
  cprintf(
    "pid: %d, used time quantum: %d, level: %d\n", 
    p->pid, 
    p->mlfq_info.level >= 0 && p->mlfq_info.level < 3 ? mlfq->MAX_TIME_QUANTUM[p->mlfq_info.level] - p->mlfq_info.tick_left : -1, 
    p->mlfq_info.level
  );
}

void
print_p_info(struct proc* p) {
  cprintf(
    "pid: %d, time quantum left: %d, level: %d, state: %d\n", 
    p->pid, 
    p->mlfq_info.tick_left, 
    p->mlfq_info.level,
    p->state
  );
}

void
init_mlfq(MLFQ* mlfq, struct proc* ptable_procs) {
  int i;
  struct proc* iter_ptr;
  
  mlfq->state = IDLE;
  mlfq->ptable_ptr = ptable_procs;
  mlfq->l2q_enter_id = 0;
  mlfq->locked_proc = NULL_;

  for (i = 0; i < NMLFQLEVEL; ++i) {
    mlfq->sched_queue[i].head = NULL_;    // NULL_ when empty
    mlfq->sched_queue[i].tail = NULL_;    // NULL_ when empty
  }

  for (iter_ptr = mlfq->ptable_ptr; iter_ptr < &((mlfq->ptable_ptr)[NPROC]); iter_ptr++) {
    iter_ptr->mlfq_info.prev = NULL_;  // NULL_ when no prev (I'm the head element)
    iter_ptr->mlfq_info.next = NULL_;  // NULL_ when no next (I'm the tail element)
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

  p->mlfq_info.prev = NULL_;
  p->mlfq_info.next = NULL_;

  return;
}

void
push_head(QList* queue, struct proc* p) {
  struct proc* temp;

  if (queue->head == NULL_) {
    push_first_elem(queue, p);

    return;
  }
  
  temp = queue->head;
  queue->head = p;

  temp->mlfq_info.prev = p;

  p->mlfq_info.next = temp;
  p->mlfq_info.prev = NULL_;
}

void push_tail(QList* queue, struct proc* p) {
  struct proc* temp;

  if (queue->head == NULL_) {
    push_first_elem(queue, p);

    return;
  }

  temp = queue->tail;
  queue->tail = p;
  
  temp->mlfq_info.next = p;

  p->mlfq_info.prev = temp;
  p->mlfq_info.next = NULL_;
}

void
push_by_priority(QList* queue, struct proc* p) {
  int found = FALSE;

  struct proc* iter_ptr;
  struct proc* temp;

  if (queue->head == NULL_) {
    push_first_elem(queue, p);
    
    return;
  }

  // iterate from head
  for (iter_ptr = queue->head; iter_ptr != NULL_ && !found; iter_ptr = iter_ptr->mlfq_info.next) {
    if (compare_priority(p, iter_ptr) <= 0) {
      // insert before target
      temp = iter_ptr->mlfq_info.prev;
      iter_ptr->mlfq_info.prev = p;
      p->mlfq_info.next = iter_ptr;
      p->mlfq_info.prev = temp;

      if (temp == NULL_) {
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

  if (tail_proc->mlfq_info.prev == NULL_) { // if tail was head
    queue->head = NULL_;
    return tail_proc;
  }

  tail_proc->mlfq_info.prev->mlfq_info.next = NULL_;
  tail_proc->mlfq_info.prev = NULL_;
  return tail_proc;
}

// insert process in queue.
// initialize tick 
void 
insert_queue(MLFQ* mlfq, struct proc* p, int level, int set_priority, int set_timequantum) {
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

  if (p->mlfq_info.next == NULL_) { // if p is tail
    *(tail_ptr) = p->mlfq_info.prev;
  } else {
    p->mlfq_info.next->mlfq_info.prev = p->mlfq_info.prev;
  }

  if (p->mlfq_info.prev == NULL_) { // if p is head
    *(head_ptr) = p->mlfq_info.next;
  } else {
    p->mlfq_info.prev->mlfq_info.next = p->mlfq_info.next;
  }

  p->mlfq_info.prev = NULL_;
  p->mlfq_info.next = NULL_;
}

// return the level of queue which can be scheduled next.
// check from L0 first, and L1, L2 sequentially.
// panic when there's no queue can be scheduled.
int
get_able_queue(MLFQ* mlfq) {
  int i;
  
  for (i = 0; i < NMLFQLEVEL; i++) {
    if (mlfq->sched_queue[i].head != NULL_) {
      return i;
    }
  }

  //panic("No queue can be scheduled");
  return -1;
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
  if (target_level == -1) {
    return NULL_;
  }
  
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
  struct proc* target_proc = NULL_;
  
  for (iter_ptr = mlfq->ptable_ptr; iter_ptr < &((mlfq->ptable_ptr)[NPROC]); iter_ptr++) {
    if (iter_ptr->pid == pid) {
      target_proc = iter_ptr;
      break;
    }
  }

  if (target_proc == NULL_) {
    cprintf("pid: %d", pid);
    panic("couldn't find the pid");
  }

  if (target_proc->state != RUNNABLE && target_proc->state != SLEEPING) {
    if (target_proc->state == RUNNING){
      target_proc->mlfq_info.priority.pvalue = priority;
      return;
    }
    print_p_info(target_proc);
    panic("target proc not RUNNABLE or SLEEPING so couldn't relocate");
  }

  target_proc->mlfq_info.priority.pvalue = priority;

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
    //cprintf("boost unlock\n");
    
    target_proc = mlfq->locked_proc;
    mlfq->locked_proc = NULL_;

    target_proc->mlfq_info.level = L0;

    push_tail(&(mlfq->sched_queue[L0]), target_proc);
  }

  while (L1Q_ptr->head != NULL_) {
    target_proc = pop_tail(L1Q_ptr);
    insert_queue(mlfq, target_proc, L0, TRUE, TRUE);
  }

  while (L2Q_ptr->head != NULL_) {
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
  mlfq->global_tick = 0;
  mlfq->locked_proc = target_proc;
}

void
scheduler_unlock(MLFQ* mlfq) {
  struct proc* target_proc;

  if (mlfq->state != LOCKED) {
    cprintf("mlfq state: %d\n", mlfq->state);
    panic("mlfq is not locked");
  }
  if (mlfq->locked_proc == NULL_) {
    panic("no process locked");
  }

  mlfq->state = UNLOCK_REQUIRE;

  target_proc = mlfq->locked_proc;
  mlfq->locked_proc = NULL_;

  target_proc->mlfq_info.priority.pvalue = MLFQMAXPRIORIY;
  target_proc->mlfq_info.tick_left = mlfq->MAX_TIME_QUANTUM[L0];
  target_proc->mlfq_info.level = L0;
}

void
check_lock_state_when_sched(MLFQ* mlfq, struct proc* p) {
  if (mlfq->state == LOCKED) {
    if (p->state == SLEEPING && p == mlfq->locked_proc) {
      scheduler_unlock(mlfq);
      mlfq->state = IDLE;
      return;
    }

    if (p->state == ZOMBIE) {
      scheduler_unlock(mlfq);
      mlfq->state = IDLE;
      return;
    }
  }
}

void
check_wakeup(MLFQ* mlfq, struct proc* p) {
  insert_queue(mlfq, p, p->mlfq_info.level, FALSE, TRUE);
}