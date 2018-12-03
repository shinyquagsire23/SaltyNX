#include <switch.h>

#include <string.h>
#include <stdio.h>

#include "useful.h"
#include "ipc_handoffs.h"
#include "loadelf.h"
#include "saltysd_proc_elf.h"

u32 __nx_applet_type = AppletType_None;

static char g_heap[0x20000];

void __libnx_initheap(void)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];
}

void __appInit(void)
{
    Result ret;
    smInitialize();
    
    write_log("SaltySD Spawner: waiting for SD\n");
    
    // Wait for SD card to be online
    for (int i = 0; i < 7; i++)
    {
        svcSleepThread(1*1000*1000*1000);
    }
    
    write_log("SaltySD Spawner: getting SD\n");
    
    fsInitialize();
    fsdevMountSdmc();
    
    write_log("SaltySD Spawner: got SD card\n");
}

void __appExit(void)
{
    fsdevUnmountAll();
    fsExit();
    smExit();
}

int main(int argc, char *argv[])
{
    Result ret;

    write_log("SaltySD Spawner Start\n");
    ret = load_elf(saltysd_proc_elf, saltysd_proc_elf_size);
    if (ret)
        write_log("Spawner: ELF load failed with %x\n", ret);

    ret = ipc_handoffs();
    if (ret)
        write_log("Spawner: IPC handoffs returned %x\n", ret);

    write_log("Spawner: Goodbye.\n");

    return 0;
}

