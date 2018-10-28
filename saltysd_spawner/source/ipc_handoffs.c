#include "ipc_handoffs.h"

#include <switch/kernel/ipc.h>

#include "useful.h"

void ipc_handoffs()
{
    Result ret;
    Handle port;

    // Wait for processes to spin up
    for (int i=  0; i < 5; i++)
    {
        svcSleepThread(1*1000*1000*1000);
    }
    
    // Send over handles to spawned process
}
