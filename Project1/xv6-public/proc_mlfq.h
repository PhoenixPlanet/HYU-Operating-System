typedef enum {
  IDLE = 0,
  LOCKED,
  UNLOCK_REQUIRE
} MLFQState;

typedef struct _QList {
  struct proc* head;               // NULL_ when queue is empty
  struct proc* tail;               // NULL_ when queue is empty
} QList;

typedef struct _MLFQ {
  const int MAX_TIME_QUANTUM[NMLFQLEVEL];

  MLFQState state;                 // if scheduler locked(LOCKED) or not(IDLE)
  struct proc* ptable_ptr;         // implement queue as linked list

  QList sched_queue[NMLFQLEVEL];   // Linked list for L0~L2 queue
  int l2q_enter_id;                // increase 1 when new process enter, 0 is default

  struct proc* locked_proc;        // process that has called schedulerLock()
} MLFQ;