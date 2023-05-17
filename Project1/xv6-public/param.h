#define NPROC        64  // maximum number of processes
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks

#define NMLFQLEVEL    3  // number of mlfq level
#define MLFQL0TIMEQ   4  // mlfq level 0 time quantum
#define MLFQL1TIMEQ   6  // mlfq level 0 time quantum
#define MLFQL2TIMEQ   8  // mlfq level 0 time quantum
#define L0            0  // RR Scheduling
#define L1            1  // RR Scheduling
#define L2            2  // Priority Scheduling

#define MLFQMAXPRIORIY 3 // mlfq max priority
#define MLFQBOOSTTIME 100 // mlfq boost time

#define MLFQLOCKPASSWORD 2019039843 // password for mlfq lock

#define NULL_         0
#define TRUE          1
#define FALSE         0
