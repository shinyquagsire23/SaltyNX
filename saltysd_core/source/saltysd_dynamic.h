#ifndef SALTYSD_DYNAMIC_H
#define SALTYSD_DYNAMIC_H

#include <stdint.h>

#include "useful.h"

uint64_t SaltySDCore_GetSymbolAddr(void* base, char* name) LINKABLE;
uint64_t SaltySDCore_FindSymbol(char* name) LINKABLE;
void SaltySDCore_RegisterModule(void* base) LINKABLE;
void SaltySDCore_DynamicLinkModule(void* base) LINKABLE;

#endif // SALTYSD_DYNAMIC_H
