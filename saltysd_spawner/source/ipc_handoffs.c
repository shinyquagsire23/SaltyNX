#include "ipc_handoffs.h"

#include <switch/kernel/ipc.h>

#include "useful.h"

bool should_terminate = false;

Result fsp_init(Service fsp)
{
    Result rc;
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        u64 unk;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 1;
    raw->unk = 0;

    rc = serviceIpcDispatch(&fsp);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        rc = resp->result;
    }
    
    return rc;
}

Result fsp_getSdCard(Service fsp, Handle* out)
{
    Result rc;
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 18;

    rc = serviceIpcDispatch(&fsp);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        *out = r.Handles[0];

        rc = resp->result;
    }
    
    return rc;
}

Result handle_cmd(int cmd)
{
    Result ret = 0;

    // Send reply
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    if (cmd == 1)
    {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 command;
            char name[12];
            u32 reserved;
        } *resp = r.Raw;
        
        char name[12];
        u64 pid = r.Pid;
        memcpy(name, resp->name, 12);

        SaltySD_printf("Spawner: SaltySD (pid %x) asked for handle %s\n", pid, name);

        if (!strcmp(name, "sdcard"))
        {
            Service toget;
            Handle sdcard;
            ret = smGetService(&toget, "fsp-srv");
            if (ret)
                SaltySD_printf("Spawner: couldn't get fsp-srv handle, ret %x\n", ret);
            
            if (!ret)
            {
                ret = fsp_init(toget);
                if (ret)
                    SaltySD_printf("Spawner: couldn't init fsp, ret %x\n", ret);
            }

            if (!ret)
                ret = fsp_getSdCard(toget, &sdcard);
            
            if (!ret)
                ipcSendHandleMove(&c, sdcard);
            else
                SaltySD_printf("Spawner: couldn't get SdCard handle, ret %x\n", ret);
        }
        else
        {
            Service toget;
            ret = smGetService(&toget, name);
            if (!ret && !strcmp(name, "fsp-srv"))
            {
                ret = fsp_init(toget);
            }

            if (!ret)
                ipcSendHandleMove(&c, toget.handle);
            else
                SaltySD_printf("Spawner: couldn't get handle, ret %x\n", ret);
        }
    }
    else if (cmd == 2)
    {
        should_terminate = true;
        ret = 0;
    }
    else
    {
        ret = 0xEE01;
    }
    
    struct {
        u64 magic;
        u64 result;
        u64 reserved[2];
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = ret;
}

void saltysd_test(Handle port)
{
    Result ret;

    // Send a command
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        u64 reserved[2];
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 0;

    SaltySD_printf("Spawner: sending SaltySD test\n");
    ret = ipcDispatch(port);

    if (R_SUCCEEDED(ret)) {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
            u64 reserved[2];
        } *resp = r.Raw;

        ret = resp->result;
    }
}

Result ipc_handoffs()
{
    Result ret;
    Handle port;
    Service fsp;
    
    ret = svcManageNamedPort(&port, "Spawner", 1);
    
    while (1)
    {
        Handle session;
        ret = svcAcceptSession(&session, port);
        if (ret && ret != 0xf201)
        {
            //SaltySD_printf("Spawner: svcAcceptSession returned %x\n", ret);
        }
        else if (!ret)
        {
            int handle_index;
            int reply_num = 0;
            Handle replySession = 0;
            while (1)
            {
                ret = svcReplyAndReceive(&handle_index, &session, 1, replySession, U64_MAX);
                
                if (should_terminate) break;
                
                if (ret) break;
                
                IpcParsedCommand r;
                ipcParse(&r);

                struct {
                    u64 magic;
                    u64 command;
                    u64 reserved[2];
                } *resp = r.Raw;

                handle_cmd(resp->command);

                replySession = session;
                svcSleepThread(1000*1000);
            }
            
            svcCloseHandle(session);
        }

        if (should_terminate) break;
        svcSleepThread(1000*1000);
    }
    
    svcCloseHandle(port);
    
    // Test SaltySD commands
    Service saltysd;
    do
    {
        ret = svcConnectToNamedPort(&saltysd, "SaltySD");
        svcSleepThread(1000*1000);
    }
    while (ret);
    saltysd_test(saltysd.handle);
    svcCloseHandle(saltysd.handle);

    return ret;
}
