#include "ipc_handoffs.h"

#include <switch.h>

#include "useful.h"

bool should_terminate = false;

Result fsp_init()
{
    Result rc;
    rc = fsInitialize();
	return rc;
}

Result fsp_getSdCard()
{
    Result rc;
    rc = fsdevMountSdmc();
	return rc;
}

Result ipc_handoffs()
{
    Result ret;
    Handle port;
    
    ret = svcManageNamedPort(&port, "Spawner", 1);
    
    while (1)
    {
        Handle session;
        ret = svcAcceptSession(&session, port);
        if (ret && ret != 0xf201)
        {
            SaltySD_printf("Spawner: svcAcceptSession returned %x\n", ret);
        }
        else if (!ret)
        {
            int handle_index;
            Handle replySession = 0;
            while (1)
            {
                ret = svcReplyAndReceive(&handle_index, &session, 1, replySession, UINT64_MAX);
                
                if (!ret) break;
            }
            
            svcCloseHandle(session);
        }
        svcSleepThread(1000*1000);
    }
    
    svcCloseHandle(port);
    
    // Test SaltySD commands
    Handle saltysd;
    do
    {
        ret = svcConnectToNamedPort(&saltysd, "SaltySD");
        svcSleepThread(1000*1000);
    }
    while (ret);

    return ret;
}
