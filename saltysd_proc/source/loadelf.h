#ifndef IPC_HANDOFFS_H
#define IPC_HANDOFFS_H

#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

Result load_elf(Handle debug, uint64_t* start, uint8_t* elf_data, u32 elf_size);

#ifdef __cplusplus
}
#endif

#endif // IPC_HANDOFFS_H


