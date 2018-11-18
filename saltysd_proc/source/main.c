#include <switch.h>

#include <string.h>
#include <stdio.h>
#include <switch/kernel/ipc.h>
#include "saltysd_bootstrap_elf.h"

#include "useful.h"

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

Result get_handle(Handle port, Handle *retrieve, char* name)
{
    Result ret = 0;

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
    
    return ret;
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
        else
        {
            write_log("SaltySD: got return %x\n", ret);
        }
    }
    else
    {
        write_log("SaltySD: IPC dispatch failed, %x\n", ret);
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

void hijack_bootstrap(Handle* debug, u64 pid, u64 tid)
{
    ThreadContext context;
    Result ret;
    
    ret = svcGetDebugThreadContext(&context, *debug, tid, RegisterGroup_All);
    
    // Load in the ELF
    //svcReadDebugProcessMemory(backup, debug, context.pc.x, 0x1000);
    u8* elf = malloc(saltysd_bootstrap_elf_size);
    memcpy(elf, saltysd_bootstrap_elf, saltysd_bootstrap_elf_size);
    
    uint64_t new_start;
    load_elf(*debug, &new_start, elf, saltysd_bootstrap_elf_size);
    free(elf);

    // Set new PC
    context.pc.x = new_start;
    ret = svcSetDebugThreadContext(*debug, tid, &context, RegisterGroup_All);
     
    svcCloseHandle(*debug);
}

void hijack_pid(u64 pid)
{
    Result ret;
    u32 threads;
    Handle debug;
    svcDebugActiveProcess(&debug, pid);

    u64* tids = malloc(0x200 * sizeof(u64));

    // Poll for new threads (svcStartProcess) while stuck in debug
    do
    {
        ret = svcGetThreadList(&threads, tids, 0x200, debug);
        svcSleepThread(-1);
    }
    while (!threads);
    
    ThreadContext context, oldContext;
    ret = svcGetDebugThreadContext(&context, debug, tids[0], RegisterGroup_All);
    oldContext = context;

    write_log("SaltySD: new max %llx, %x %016llx\n", pid, threads, context.pc.x);
            
    hijack_bootstrap(&debug, pid, tids[0]);
    
    free(tids);
}

int main(int argc, char *argv[])
{
    Result ret;
    Handle port, saltyport, pmdmnt, fsp;
    
    write_log("SaltySD says hello!\n");
    
    do
    {
        ret = svcConnectToNamedPort(&port, "Spawner");
        svcSleepThread(1000*1000);
    }
    while (ret);
    
    // Begin asking for handles
    get_handle(port, &fsp, "fsp-srv");
    /*while(get_handle(port, &pmdmnt, "pm:info"))
    {
        svcSleepThread(1000*1000*10000);
    }*/
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
                should_terminate = true;

                replySession = session;
                svcSleepThread(1000*1000);
            }
            
            svcCloseHandle(session);
        }

        if (should_terminate) break;
        svcSleepThread(1000*1000);
    }
    
    svcCloseHandle(saltyport);
    
    //TODO: maybe just be silent or something idk
    for (int i = 0; i < 13; i++)
    {
        svcSleepThread(1000*1000*1000);
    }
    
    u64* pids = malloc(0x200 * sizeof(u64));
    u64 max = 0;
    while (1)
    {
        u32 num;
        svcGetProcessList(&num, pids, 0x200);

        u64 old_max = max;
        for (int i = 0; i < num; i++)
        {
            if (pids[i] > max)
            {
                max = pids[i];
            }
        }

        // Detected new PID
        if (max != old_max && max > 0x80)
        {
            hijack_pid(max);
        }

        svcSleepThread(1000*1000);
    }
    free(pids);

    return 0;
}

