#include <stdio.h>
#include "vcpu.h"
#include <stdlib.h>

extern vcpu_t vcpu;

void svc_example(void)
{
	vcpu.regs->arm_r0 = 0;
	vcpu.regs->arm_r1 = (uint32_t)0xdeadbeef;
}

void svc(int sys_id)
{
	switch (sys_id) {
	case 0x0:
		svc_example();
		break;
	default:
		vcpu.regs->arm_r0 = 0x0;
		return;
	}
}
