#define _GNU_SOURCE

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/ucontext.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include "vcpu.h"

#define HOST_EXCP_STACK_SIZE 1*1024*1024

vcpu_t vcpu;
void svc(int sys_id);

void dispatch_memaccess(int signo, siginfo_t *info, void *context)
{
	if (vcpu.running ^= 1) {
		printf("Double fault! access was jump?");
		exit(2);
	}
	vcpu.regs = (struct sigcontext *) &((ucontext_t *)context)->uc_mcontext;
	printf("access = 0x%x @ pc = 0x%x\n", (uintptr_t)info->si_addr, (uintptr_t)vcpu.regs->arm_pc);
        vcpu.regs = (struct sigcontext *) &((ucontext_t *)context)->uc_mcontext;

	if(*(uint32_t*)vcpu.regs->arm_pc & (1 << 26)) {
		if(*(uint32_t*)vcpu.regs->arm_pc & (1 << 22)) {
				/* printf("Access is unsigned\n"); */
				/* printf("Access is 1B\n"); */
				if(*(uint32_t*)vcpu.regs->arm_pc & (1 << 20)) {
					/* printf("Access is Load\n"); */
					/*TODO: move the read value to register*/
				        vcpu.read8(0);
				} else {
					/* printf("Access is Store\n"); */
					vcpu.write8(0);
				}
		} else {
			/* printf("Access is 4B\n"); */
			if(*(uint32_t*)vcpu.regs->arm_pc & (1 << 20)) {
				/* printf("Access is Load\n"); */
				vcpu.read32();
			} else {
				/* printf("Access is Store\n"); */
				vcpu.write32();
			}
		}
	} else {
		if(*(uint32_t*)vcpu.regs->arm_pc & (1 << 5)) {
			/* printf("Access is halfword\n"); */
			if(*(uint32_t*)vcpu.regs->arm_pc & (1 << 20)) {
				/* printf("Access is Load\n"); */
				if(!*(uint32_t*)vcpu.regs->arm_pc & (1 << 6)) {
					/* printf("Access is unsigned\n"); */
					vcpu.read16(0);
				} else {
					/* printf("Access is signed\n"); */
					vcpu.read16(1);
				}
			} else {
				/* printf("Access is Store\n"); */
				if(!*(uint32_t*)vcpu.regs->arm_pc & (1 << 6)) {
					/* printf("Access is unsigned\n"); */
					vcpu.write16(0);
				} else {
					/* printf("Access is signed\n"); */
					vcpu.write16(1);
				}

			}
		} else {
			/* printf("Access is 1B\n"); */
			/* printf("Access is signed\n"); */
			if(*(uint32_t*)vcpu.regs->arm_pc & (1 << 20)) {
				/* printf("Access is Load\n"); */
				vcpu.read8(1);
			} else {
				/* printf("Access is Store\n"); */
				vcpu.write8(1);
			}
		}


	}

	vcpu.regs->arm_pc += 4;
	vcpu.running = 1;
	return;
}

void illegal_instruction(int signo, siginfo_t *info, void *context)
{
        vcpu.regs = (struct sigcontext *) &((ucontext_t *)context)->uc_mcontext;

	if (((*(uint32_t*)info->si_addr) >> 24) != 0xef) {
		printf("Not a svc. %x\n", (*(uint32_t*)info->si_addr));
		exit(1);
	} else {
		printf("SYSCALL #x%x\n", ((*(uint32_t*)info->si_addr) & 0x00ffffff));
		svc(((*(uint32_t*)info->si_addr) & 0x00ffffff));
	}
	vcpu.regs->arm_pc = ((uint32_t)info->si_addr) + 4;
}

void dispatch_sigbus(int signo, siginfo_t *info, void *context)
{
        vcpu.regs = (struct sigcontext *) &((ucontext_t *)context)->uc_mcontext;

	printf("SIGBUS %x @ %lx\n", (uint32_t)info->si_addr, vcpu.regs->arm_pc);
	exit(1);
}
void nop()
{
	return;
}
void run_vcpu(uint32_t entry, uint32_t stack)
{
	ucontext_t new_context;
	struct sigaction act = { 0 };
	stack_t ss;

	vcpu.read8 = (uint32_t (*)(uint8_t))nop;
	vcpu.read16 = (uint32_t (*)(uint8_t))nop;
	vcpu.read32 = (uint32_t (*)(void))nop;
	vcpu.write8 = (void (*)(uint8_t))nop;
	vcpu.write16 = (void (*)(uint8_t))nop;
	vcpu.write32 = (void (*)(void))nop;
	vcpu.svc = svc;

	ss.ss_sp = malloc(HOST_EXCP_STACK_SIZE);
	ss.ss_size = HOST_EXCP_STACK_SIZE;
	ss.ss_flags = 0;
	if (sigaltstack(&ss, NULL) == -1) {
		perror("sigaltstack");
		exit(EXIT_FAILURE);
	}

	act.sa_flags = SA_SIGINFO | SA_ONSTACK;
	act.sa_sigaction = &dispatch_memaccess;
	if (sigaction(SIGSEGV, &act, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	act.sa_flags = SA_SIGINFO | SA_ONSTACK;
	act.sa_sigaction = &dispatch_sigbus;
	if (sigaction(SIGBUS, &act, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	act.sa_flags = SA_SIGINFO | SA_ONSTACK;
	act.sa_sigaction = &illegal_instruction;
	if (sigaction(SIGILL, &act, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	vcpu.running = 1;
	printf("Setting pc to %x\n", entry);
	printf("Setting sp to %x\n", stack);

	__asm__ volatile("mov sp, %0\n\t"
			 "ldr pc, [%1]\n\t"
			 :
			 : "r" (stack), "r" (&entry)
			 :
		);
}

int main (int argc, char **argv)
{
	void *vm2 = mmap((void*)0x10000000 - 0x4000,
			 0x4000, PROT_READ|PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, 0, 0);

	void *vm3 = mmap((void*)0x8000000 - 0x4000,
			 0x4000, PROT_READ|PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, 0, 0);

        uint32_t entry = 0x1000; /* load_and_reloc(argv[1]); */

	run_vcpu(entry, (uintptr_t)0x10000000);
	return 0;
}
