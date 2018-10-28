#include <switch.h>

#include <string.h>
#include <stdio.h>
#include <switch/kernel/ipc.h>

u32 __nx_applet_type = AppletType_None;

static char g_heap[0x20000];
bool should_terminate = false;

void __libnx_initheap(void)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];
}

void __appInit(void)
{
    smInitialize();
}

#define write_log(...) \
    {char log_buf[0x200]; snprintf(log_buf, 0x200, __VA_ARGS__); \
    svcOutputDebugString(log_buf, strlen(log_buf));}


void get_handle(Handle port, Handle *retrieve, char* name)
{
    Result ret;

    // Send a command
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        char name[12];
        u32 reserved;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 1;
    strncpy(raw->name, name, 12);

    write_log("SaltySD: sending IPC request for handle %s\n", name);
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
        //write_log("SaltySD: Got reply %x\n", resp->result);
        
        if (!ret)
        {
            write_log("SaltySD: bind handle %x to %s\n", r.Handles[0], name);
            smAddOverrideHandle(smEncodeName(name), r.Handles[0]);
        }
    }
    else
    {
        //write_log("SaltySD: IPC dispatch failed, %x\n", ret);
    }
}

void get_port(Handle port, Handle *retrieve, char* name)
{
    Result ret;

    // Send a command
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        char name[12];
        u32 reserved;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 3;
    strncpy(raw->name, name, 12);

    write_log("SaltySD: sending IPC request for port %s\n", name);
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
        //write_log("SaltySD: Got reply %x\n", resp->result);
        
        if (!ret)
        {
            write_log("SaltySD: got handle %x for port %s\n", r.Handles[0], name);
            *retrieve = r.Handles[0];
        }
    }
    else
    {
        //write_log("SaltySD: IPC dispatch failed, %x\n", ret);
    }
}

void terminate_spawner(Handle port)
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
    raw->cmd_id = 2;

    write_log("SaltySD: terminating spawner\n");
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

int main(int argc, char *argv[])
{
    Result ret;
    Handle port, saltyport, sm, fsp;
    
    write_log("SaltySD says hello!\n");
    
    do
    {
        ret = svcConnectToNamedPort(&port, "Spawner");
        svcSleepThread(1000*1000);
    }
    while (ret);
    
    // Begin asking for handles
    get_handle(port, &fsp, "fsp-srv");
    get_port(port, &saltyport, "SaltySD");
    terminate_spawner(port);
    svcCloseHandle(port);
    
    
    while (1)
    {
        Handle session;
        ret = svcAcceptSession(&session, saltyport);
        if (ret && ret != 0xf201)
        {
            //write_log("SaltySD: svcAcceptSession returned %x\n", ret);
        }
        else if (!ret)
        {
            //write_log("SaltySD: session %x being handled\n", session);

            int handle_index;
            int reply_num = 0;
            Handle replySession = 0;
            while (1)
            {
                ret = svcReplyAndReceive(&handle_index, &session, 1, replySession, U64_MAX);
                
                if (should_terminate) break;
                
                //write_log("SaltySD: IPC reply ret %x, index %x, sess %x\n", ret, handle_index, session);
                if (ret) break;
                
                IpcParsedCommand r;
                ipcParse(&r);

                struct {
                    u64 magic;
                    u64 command;
                    u64 reserved[2];
                } *resp = r.Raw;

                write_log("SaltySD: command %x\n", resp->command);

                replySession = session;
                svcSleepThread(1000*1000);
            }
            
            svcCloseHandle(session);
        }

        if (should_terminate) break;
        svcSleepThread(1000*1000);
    }
    
    svcCloseHandle(saltyport);

    return 0;
}

