// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
enum threadstate { T_UNUSED, T_ALLOCATED, T_USING, T_ZOMBIE };

typedef struct _TNode {
  enum threadstate state;      // State of thread
  struct proc* thread;         // Pointer of thread proc struct
  uint ustack_bottom;          // Base address of thread stack page
  void* retval;                // Return value of thread
} TNode;

// Per-process state
struct proc {
  //-------- Shared data among threads (only main thread has valid value) -----------
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  int main_stack_page_num;
  uint main_stack_bottom;
  
  int memory_limit;
  int thread_num;
  TNode thread_table[NPROC];
  //-------- Shared data among threads (only main thread has valid value) -----------

  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan

  struct {         
    bool is_main;              // TRUE when this is main thread
    struct proc* main_ptr;
    thread_t thread_id;
  } thread_info;
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
