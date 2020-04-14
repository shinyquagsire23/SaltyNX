#include <switch.h>

#include <string.h>
#include <stdio.h>

#include "useful.h"
#include "ipc_handoffs.h"
#include "loadelf.h"

u32 __nx_applet_type = AppletType_None;

static char g_heap[0x10000];

void __libnx_initheap(void)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];
}

void __appInit(void)
{
    
    svcSleepThread(1*1000*1000*1000);
    smInitialize();
    
    debug_log("SaltySD Spawner: waiting for SD\n");
    
    // Wait for SD card to be online
    for (int i = 0; i < 7; i++)
    {
        svcSleepThread(1*1000*1000*1000);
    }
    
    debug_log("SaltySD Spawner: getting SD\n");
    
    fsInitialize();
    fsdevMountSdmc();
    
    SaltySD_printf("SaltySD Spawner: got SD card\n");
}

void __appExit(void)
{
    fsdevUnmountAll();
    fsExit();
    smExit();
}

Result load_elf(char* path);

int main(int argc, char *argv[])
{
    Result ret;

    SaltySD_printf("SaltySD Spawner Start\n");
    ret = load_elf("sdmc:/SaltySD/saltysd_proc.elf");
    if (ret)
        SaltySD_printf("Spawner: ELF load failed with %x\n", ret);

    ret = ipc_handoffs();
    if (ret)
        SaltySD_printf("Spawner: IPC handoffs returned %x\n", ret);

    SaltySD_printf("Spawner: Goodbye.\n");

    return 0;
}

