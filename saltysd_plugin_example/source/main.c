#include <switch.h>

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/iosupport.h>
#include <sys/reent.h>
#include <switch/kernel/ipc.h>

#include "useful.h"

#include "saltysd_core.h"
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"

u32 __nx_applet_type = AppletType_None;

static char g_heap[0x8000];

Handle orig_main_thread;
void* orig_ctx;
void* orig_saved_lr;

void __libnx_init(void* ctx, Handle main_thread, void* saved_lr)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];
    
    orig_ctx = ctx;
    orig_main_thread = main_thread;
    orig_saved_lr = saved_lr;
    
    // Call constructors.
    void __libc_init_array(void);
    __libc_init_array();
}

void __attribute__((weak)) NORETURN __libnx_exit(int rc)
{
    // Call destructors.
    void __libc_fini_array(void);
    __libc_fini_array();

    write_log("SaltySD Plugin: jumping to %p\n", orig_saved_lr);

    __nx_exit(0, orig_saved_lr);
}

extern uint64_t _ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm(uint64_t idk1, uint64_t idk2, uint64_t idk3, uint64_t idk4, uint64_t idk5) LINKABLE;
extern uint64_t _ZN2nn2fs8ReadFileENS0_10FileHandleElPvm(uint64_t idk1, uint64_t idk2, uint64_t idk3, uint64_t idk4) LINKABLE;

uint64_t ReadFile_intercept(uint64_t idk1, uint64_t idk2, uint64_t idk3, uint64_t idk4, uint64_t idk5)
{
    write_log("SaltySD Plugin: ReadFile(%llx, %llx, %llx, %llx, %llx)\n", idk1, idk2, idk3, idk4, idk5);
    return _ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm(idk1, idk2, idk3, idk4, idk5);
}

uint64_t ReadFile_intercept2(uint64_t handle, uint64_t offset, uint64_t out, uint64_t size)
{
    write_log("SaltySD Plugin: ReadFile2(%llx, %llx, %llx, %llx)\n", handle, offset, out, size);
    return _ZN2nn2fs8ReadFileENS0_10FileHandleElPvm(handle, offset, out, size);
}

int main(int argc, char *argv[])
{
    write_log("SaltySD Plugin: alive\n");
    
    char* ver = "Ver. %d.%d.%d";
    u64 dst_3 = SaltySDCore_findCode(ver, strlen(ver));
    if (dst_3)
    {
        SaltySD_Memcpy(dst_3, "SaltySD yeet", 13);
    }
    
    SaltySDCore_ReplaceImport("_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm", ReadFile_intercept);
    SaltySDCore_ReplaceImport("_ZN2nn2fs8ReadFileENS0_10FileHandleElPvm", ReadFile_intercept2);

    __libnx_exit(0);
}

