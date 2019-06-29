#ifndef SALTYSD_CORE_H
#define SALTYSD_CORE_H

#include <switch_min.h>

#include "useful.h"

#ifdef __cplusplus
extern "C" {
#endif

extern u64 SaltySDCore_getCodeStart() LINKABLE;
extern u64 SaltySDCore_getCodeSize() LINKABLE;
extern u64 SaltySDCore_findCode(u8* code, size_t size) LINKABLE;
extern FILE* SaltySDCore_fopen(const char* filename, const char* mode) LINKABLE;
extern size_t SaltySDCore_fread(void* ptr, size_t size, size_t count, FILE* stream) LINKABLE;
extern int SaltySDCore_fclose(FILE* stream) LINKABLE;

#ifdef __cplusplus
}
#endif

#endif // SALTYSD_CORE_H
