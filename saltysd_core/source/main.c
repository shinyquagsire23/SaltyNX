#include <switch_min.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <dirent.h>
#include <sys/iosupport.h>
#include <sys/reent.h>
#include <switch_min/kernel/ipc.h>
#include <switch_min/runtime/threadvars.h>
#include <stdlib.h>

#include "useful.h"
#include "saltysd_ipc.h"
#include "saltysd_core.h"
#include "saltysd_dynamic.h"

#include "bm.h"

u32 __nx_applet_type = AppletType_None;

static char g_heap[0x20000];

extern void __nx_exit_clear(void* ctx, Handle thread, void* addr);
extern void elf_trampoline(void* context, Handle thread, void* func);
void* __stack_tmp;

Handle orig_main_thread;
void* orig_ctx;

Handle sdcard;
size_t elf_area_size = 0x80000;

ThreadVars vars_orig;
ThreadVars vars_mine;

void __libnx_init(void* ctx, Handle main_thread, void* saved_lr)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];

    orig_ctx = ctx;
    orig_main_thread = main_thread;
    
    // Hacky TLS stuff, TODO: just stop using libnx t b h
    vars_mine.magic = 0x21545624;
    vars_mine.handle = main_thread;
    vars_mine.thread_ptr = NULL;
    vars_mine.reent = _impure_ptr;
    vars_mine.tls_tp = (void*)malloc(0x1000);
    vars_orig = *getThreadVars();
    *getThreadVars() = vars_mine;
    
    // Call constructors.
    void __libc_init_array(void);
    __libc_init_array();
}

void __attribute__((weak)) __libnx_exit(int rc)
{
    fsdevUnmountAll();

    // Call destructors.
    void __libc_fini_array(void);
    __libc_fini_array();
    
    // Restore TLS stuff
    *getThreadVars() = vars_orig;
    
    u64 addr = SaltySDCore_getCodeStart();

    __nx_exit_clear(orig_ctx, orig_main_thread, (void*)addr);
}

u64  g_heapAddr;
size_t g_heapSize;

void setupELFHeap(void)
{
    void* addr = NULL;
    Result rc = 0;

    rc = svcSetHeapSize(&addr, ((elf_area_size+0x200000+0x200000) & 0xffe00000));

    if (rc || addr == NULL)
    {
        debug_log("SaltySD Bootstrap: svcSetHeapSize failed with err %x\n", rc);
    }

    g_heapAddr = (u64)addr;
    g_heapSize = ((elf_area_size+0x100000+0x200000) & 0xffe00000);
}

u64 find_next_elf_heap()
{
    u64 addr = g_heapAddr;
    while (1)
    {
        MemoryInfo info;
        u32 pageinfo;
        Result ret = svcQueryMemory(&info, &pageinfo, addr);
        
        if (info.perm == Perm_Rw)
            return info.addr;

        addr = info.addr + info.size;
        
        if (!addr || ret) break;
    }
    
    return 0;
}

extern void _start();

void SaltySDCore_RegisterExistingModules()
{
    u64 addr = 0;
    while (1)
    {
        MemoryInfo info;
        u32 pageinfo;
        Result ret = svcQueryMemory(&info, &pageinfo, addr);
        
        if (info.perm == Perm_Rx)
        {
            SaltySDCore_RegisterModule((void*)info.addr);
			u64 compaddr = (u64)info.addr;
            if ((u64*)compaddr != (u64*)_start)
                SaltySDCore_RegisterBuiltinModule((void*)info.addr);
        }

        addr = info.addr + info.size;
        
        if (!addr || ret) break;
    }
    
    return;
}

Result svcSetHeapSizeIntercept(u64 *out, u64 size)
{
    Result ret = svcSetHeapSize((void*)out, size+((elf_area_size+0x200000) & 0xffe00000));
    
    //SaltySD_printf("SaltySD Core: svcSetHeapSize intercept %x %llx %llx\n", ret, *out, size+((elf_area_size+0x200000) & 0xffe00000));
    
    if (!ret)
    {
        *out += elf_area_size;
    }
    
    return ret;
}

Result svcGetInfoIntercept (u64 *out, u64 id0, Handle handle, u64 id1)
{
    Result ret = svcGetInfo(out, id0, handle, id1);
    
    //SaltySD_printf("SaltySD Core: svcGetInfo intercept %p (%llx) %llx %x %llx ret %x\n", out, *out, id0, handle, id1, ret);
    
    if (id0 == 6 && id1 == 0 && handle == 0xffff8001)
    {
        *out -= elf_area_size;
    }
    
    return ret;
}

void SaltySDCore_PatchSVCs()
{
    Result ret;
    static u8 orig_1[0x8] = {0xE0, 0x0F, 0x1F, 0xF8, 0x21, 0x00, 0x00, 0xD4};
    static u8 orig_2[0x8] = {0xE0, 0x0F, 0x1F, 0xF8, 0x21, 0x05, 0x00, 0xD4};
    const u8 nop[0x4] = {0x1F, 0x20, 0x03, 0xD5};
    static u8 patch[0x10] = {0x44, 0x00, 0x00, 0x58, 0x80, 0x00, 0x1F, 0xD6, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0};
    u64 dst_1 = SaltySDCore_findCode(orig_1, 8);
    u64 dst_2 = SaltySDCore_findCode(orig_2, 8);
    
    if (!dst_1 || !dst_2)
    {
        SaltySD_printf("SaltySD Core: Failed to find svcGetInfo and svcGetHeapSize! %llx, %llx\n", dst_1, dst_2);
        return;
    }

    *(u64*)&patch[8] = (u64)svcSetHeapSizeIntercept;
    if (dst_1 & 4)
    {
        ret = SaltySD_Memcpy(dst_1, (u64)nop, 0x4);
        if (ret)
        {
            debug_log("svcSetHeapSize memcpy failed!\n");
        }
        else
        {
            ret = SaltySD_Memcpy(dst_1+4, (u64)patch, 0x10);
        }
    }
    else
    {
        ret = SaltySD_Memcpy(dst_1, (u64)patch, 0x10);
    }
    if (ret) debug_log("svcSetHeapSize memcpy failed!\n");
    
    *(u64*)&patch[8] = (u64)svcGetInfoIntercept;
    if (dst_2 & 4)
    {
        ret = SaltySD_Memcpy(dst_2, (u64)nop, 0x4);
        if (ret)
        {
            debug_log("svcSetHeapSize memcpy failed!\n");
        }
        else
        {
            ret = SaltySD_Memcpy(dst_2+4, (u64)patch, 0x10);
        }
    }
    else
    {
        ret = SaltySD_Memcpy(dst_2, (u64)patch, 0x10);
    }
    if (ret) debug_log("svcSetHeapSize memcpy failed!\n");
}

void** SaltySDCore_LoadPluginsInDir(char* path, void** entries, size_t* num_elfs)
{
    char* tmp = malloc(0x100);
    DIR *d;
    struct dirent *dir;

    SaltySD_printf("SaltySD Core: Searching plugin dir `%s'...\n", path);
    
    snprintf(tmp, 0x100, "sdmc:/SaltySD/plugins/%s", path);

    d = opendir(tmp);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            char *dot = strrchr(dir->d_name, '.');
            if (dot && !strcmp(dot, ".elf"))
            {
                u64 elf_addr, elf_size;
                setupELFHeap();
                
                snprintf(tmp, 0x100, "%s%s", path, dir->d_name);
                SaltySD_LoadELF(find_next_elf_heap(), &elf_addr, &elf_size, tmp);

                *num_elfs = *num_elfs + 1;
                entries = realloc(entries, *num_elfs * sizeof(void*));
                entries[*num_elfs-1] = (void*)elf_addr;

                SaltySDCore_RegisterModule(entries[*num_elfs-1]);
                
                elf_area_size += elf_size;
            }
        }
        closedir(d);
    }
    free(tmp);
    
    return entries;
}

void SaltySDCore_LoadPlugins()
{
    // Load plugin ELFs
    char* tmp = malloc(0x20);

    void** entries = NULL;
    size_t num_elfs = 0;

    uint64_t tid = 0;
    svcGetInfo(&tid, 18, CUR_PROCESS_HANDLE, 0);
    
    entries = SaltySDCore_LoadPluginsInDir("", entries, &num_elfs);
    snprintf(tmp, 0x20, "%016" PRIx64 "/", tid);
    entries = SaltySDCore_LoadPluginsInDir(tmp, entries, &num_elfs);
    
    for (int i = 0; i < num_elfs; i++)
    {
        SaltySDCore_DynamicLinkModule(entries[i]);
        elf_trampoline(orig_ctx, orig_main_thread, entries[i]);
    }
    if (num_elfs)
        free(entries);
    
    free(tmp);
}

int main(int argc, char *argv[])
{
    Result ret;

    debug_log("SaltySD Core: waddup\n");

    // Get our address space in order
    SaltySDCore_getCodeStart();
    setupELFHeap();
    elf_area_size = find_next_elf_heap() - (u64)g_heapAddr;
    setupELFHeap();
    
    SaltySDCore_RegisterExistingModules();
    
    SaltySD_Init();

    SaltySD_printf("SaltySD Core: restoring code...\n");
    ret = SaltySD_Restore();
    if (ret) goto fail;
    
    ret = SaltySD_GetSDCard(&sdcard);
    if (ret) goto fail;

    SaltySDCore_PatchSVCs();
    SaltySDCore_LoadPlugins();

    ret = SaltySD_Deinit();
    if (ret) goto fail;

    __libnx_exit(0);

fail:
    debug_log("SaltySD Core: failed with retcode %x\n", ret);
    SaltySD_printf("SaltySD Core: failed with retcode %x\n", ret);
    __libnx_exit(0);
}

