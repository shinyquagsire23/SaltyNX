#include <switch.h>

#include <string.h>
#include <stdio.h>
#include <switch/kernel/ipc.h>

#include "useful.h"

u32 __nx_applet_type = AppletType_None;

static char g_heap[0x20000];

void __libnx_init(void)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];
    
    // Call constructors.
    void __libc_init_array(void);
    __libc_init_array();
}

void __attribute__((weak)) NORETURN __libnx_exit(int rc)
{
    // Call destructors.
    void __libc_fini_array(void);
    __libc_fini_array();

    __nx_exit(0, (void*)0x123456);
}

int main(int argc, char *argv[])
{
    Result ret;
    
    write_log("SaltySD Core: waddup\n");

    while (1)
    {
        svcSleepThread(1000*1000);
    }

    return 0;
}

