#ifndef _RISCV_MCALL_H
#define _RISCV_MCALL_H

#define MCALL_HART_ID 300
#define MCALL_CONSOLE_PUTCHAR 11
#define MCALL_CONSOLE_GETCHAR 301
#define MCALL_HTIF_SYSCALL 302
#define MCALL_SEND_IPI 303
#define MCALL_CLEAR_IPI 304
#define MCALL_SHUTDOWN 305
#define MCALL_SET_TIMER 200
#define MCALL_GET_TIMER 201
#define MCALL_REMOTE_SFENCE_VM 306
#define MCALL_REMOTE_FENCE_I 307

#ifndef __ASSEMBLER__

extern uintptr_t do_mcall(uintptr_t which, ...);

#endif

#endif
