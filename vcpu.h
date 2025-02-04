#include <inttypes.h>
#include <signal.h>

typedef struct vcpu {
	struct sigcontext *regs;
	uint8_t running;
	void (*write32)();
	void (*write16)(uint8_t extend);
	void (*write8)(uint8_t extend);

        uint32_t (*read32)();
	uint32_t (*read16)(uint8_t extend);
	uint32_t (*read8)(uint8_t extend);

        void (*svc)(int sys_id);
} vcpu_t;
