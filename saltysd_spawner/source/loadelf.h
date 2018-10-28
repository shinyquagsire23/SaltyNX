#ifndef IPC_HANDOFFS_H
#define IPC_HANDOFFS_H

#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

Result load_elf(uint8_t* elf, u32 elf_Size);

#ifdef __cplusplus
}
#endif

#endif // IPC_HANDOFFS_H


