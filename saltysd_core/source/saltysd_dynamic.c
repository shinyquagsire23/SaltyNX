#include <elf.h>

#include <switch.h>
#include <elf.h>

#include "useful.h"

void** elfs = NULL;
uint32_t num_elfs = 0;

void** builtin_elfs = NULL;
uint32_t num_builtin_elfs = 0;

struct nso_header
{
    uint32_t start;
    uint32_t mod;
};

struct mod0_header
{
    uint32_t magic;
    uint32_t dynamic;
};


uint64_t SaltySDCore_GetSymbolAddr(void* base, char* name)
{
    const Elf64_Dyn* dyn = NULL;
    const Elf64_Rela* rela = NULL;
    const Elf64_Sym* symtab = NULL;
    const char* strtab = NULL;
    uint64_t relasz = 0;
    uint64_t numsyms = 0;
    
    struct nso_header* header = (struct nso_header*)base;
    struct mod0_header* modheader = (struct nso_header*)(base + header->mod);
    dyn = (const Elf64_Dyn*)((void*)modheader + modheader->dynamic);

    for (; dyn->d_tag != DT_NULL; dyn++)
    {
        switch (dyn->d_tag)
        {
            case DT_SYMTAB:
                symtab = (const Elf64_Sym*)(base + dyn->d_un.d_ptr);
                break;
            case DT_STRTAB:
                strtab = (const char*)(base + dyn->d_un.d_ptr);
                break;
            case DT_RELA:
                rela = (const Elf64_Rela*)(base + dyn->d_un.d_ptr);
                break;
            case DT_RELASZ:
                relasz = dyn->d_un.d_val / sizeof(Elf64_Rela);
                break;
        }
    }
    
    numsyms = ((void*)strtab - (void*)symtab) / sizeof(Elf64_Sym);
    
    for (int i = 0; i < numsyms; i++)
    {
        if (!strcmp(strtab + symtab[i].st_name, name) && symtab[i].st_value)
        {
            return (uint64_t)base + symtab[i].st_value;
        }
    }

    return 0;
}

uint64_t SaltySDCore_FindSymbol(char* name)
{
    if (!elfs) return 0;

    for (int i = 0; i < num_elfs; i++)
    {
        uint64_t ptr = SaltySDCore_GetSymbolAddr(elfs[i], name);
        if (ptr) return ptr;
    }
    
    return 0;
}

void SaltySDCore_RegisterModule(void* base)
{
    elfs = realloc(elfs, ++num_elfs * sizeof(void*));
    elfs[num_elfs-1] = base;
}

void SaltySDCore_RegisterBuiltinModule(void* base)
{
    builtin_elfs = realloc(builtin_elfs, ++num_builtin_elfs * sizeof(void*));
    builtin_elfs[num_builtin_elfs-1] = base;
}

void SaltySDCore_ReplaceModuleImport(void* base, char* name, void* new)
{
    const Elf64_Dyn* dyn = NULL;
    const Elf64_Rela* rela = NULL;
    const Elf64_Sym* symtab = NULL;
    const char* strtab = NULL;
    uint64_t relasz = 0;
    
    struct nso_header* header = (struct nso_header*)base;
    struct mod0_header* modheader = (struct nso_header*)(base + header->mod);
    dyn = (const Elf64_Dyn*)((void*)modheader + modheader->dynamic);

    for (; dyn->d_tag != DT_NULL; dyn++)
    {
        switch (dyn->d_tag)
        {
            case DT_SYMTAB:
                symtab = (const Elf64_Sym*)(base + dyn->d_un.d_ptr);
                break;
            case DT_STRTAB:
                strtab = (const char*)(base + dyn->d_un.d_ptr);
                break;
            case DT_RELA:
                rela = (const Elf64_Rela*)(base + dyn->d_un.d_ptr);
                break;
            case DT_RELASZ:
                relasz += dyn->d_un.d_val / sizeof(Elf64_Rela);
                break;
            case DT_PLTRELSZ:
                relasz += dyn->d_un.d_val / sizeof(Elf64_Rela);
                break;
        }
    }

    if (rela == NULL || symtab == NULL || strtab == NULL)
    {
        return;
    }
    
    uint64_t numsyms = ((void*)strtab - (void*)symtab) / sizeof(Elf64_Sym);
    
    write_log("relasz %x\n", relasz);

    int rela_idx = 0;
    for (; relasz--; rela++, rela_idx++)
    {
        if (ELF64_R_TYPE(rela->r_info) == R_AARCH64_RELATIVE) continue;
        
        uint32_t sym_idx = ELF64_R_SYM(rela->r_info);
        if (sym_idx >= numsyms) continue;

        char* rel_name = strtab + symtab[sym_idx].st_name;
        if (strcmp(name, rel_name)) continue;
        
        write_log("SaltySD Core: %x %s to %p, %llx %p\n", rela_idx, rel_name, new, rela->r_offset, base + rela->r_offset);

        Elf64_Rela replacement = *rela;
        replacement.r_addend = rela->r_addend + (uint64_t)new - SaltySDCore_FindSymbol(rel_name);

        SaltySD_Memcpy(rela, &replacement, sizeof(Elf64_Rela));
    }
}

void SaltySDCore_ReplaceImport(char* name, void* new)
{
    if (!builtin_elfs) return 0;

    for (int i = 0; i < num_builtin_elfs; i++)
    {
        SaltySDCore_ReplaceModuleImport(builtin_elfs[i], name, new);
    }
}

void SaltySDCore_DynamicLinkModule(void* base)
{
    const Elf64_Dyn* dyn = NULL;
    const Elf64_Rela* rela = NULL;
    const Elf64_Sym* symtab = NULL;
    const char* strtab = NULL;
    uint64_t relasz = 0;
    
    struct nso_header* header = (struct nso_header*)base;
    struct mod0_header* modheader = (struct nso_header*)(base + header->mod);
    dyn = (const Elf64_Dyn*)((void*)modheader + modheader->dynamic);

    for (; dyn->d_tag != DT_NULL; dyn++)
    {
        switch (dyn->d_tag)
        {
            case DT_SYMTAB:
                symtab = (const Elf64_Sym*)(base + dyn->d_un.d_ptr);
                break;
            case DT_STRTAB:
                strtab = (const char*)(base + dyn->d_un.d_ptr);
                break;
            case DT_RELA:
                rela = (const Elf64_Rela*)(base + dyn->d_un.d_ptr);
                break;
            case DT_RELASZ:
                relasz += dyn->d_un.d_val / sizeof(Elf64_Rela);
                break;
            case DT_PLTRELSZ:
                relasz += dyn->d_un.d_val / sizeof(Elf64_Rela);
                break;
        }
    }

    if (rela == NULL)
    {
        return;
    }

    for (; relasz--; rela++)
    {
        if (ELF64_R_TYPE(rela->r_info) == R_AARCH64_RELATIVE) continue;

        uint32_t sym_idx = ELF64_R_SYM(rela->r_info);
        char* name = strtab + symtab[sym_idx].st_name;

        uint64_t sym_val = (uint64_t)base + symtab[sym_idx].st_value;
        if (!symtab[sym_idx].st_value)
            sym_val = 0;

        if (!symtab[sym_idx].st_shndx && sym_idx)
            sym_val = SaltySDCore_FindSymbol(name);

        uint64_t sym_val_and_addend = sym_val + rela->r_addend;

        write_log("SaltySD Core: %x %llx->%llx %s\n", sym_idx, symtab[sym_idx].st_value + rela->r_addend, sym_val_and_addend, name);

        switch (ELF64_R_TYPE(rela->r_info))
        {
            case R_AARCH64_GLOB_DAT:
            case R_AARCH64_JUMP_SLOT:
            case R_AARCH64_ABS64:
            {
                uint64_t* ptr = (uint64_t*)(base + rela->r_offset);
                *ptr = sym_val_and_addend;
                break;
            }
        }
    }
}
