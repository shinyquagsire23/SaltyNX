#include <switch.h>

#include <string.h>
#include <stdio.h>
#include <switch/kernel/ipc.h>
#include "saltysd_bootstrap_elf.h"
#include "saltysd_core_elf.h"

#include "spawner_ipc.h"

#include "useful.h"

u32 __nx_applet_type = AppletType_None;

void serviceThread(void* buf);
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
    load_elf_debug(*debug, &new_start, elf, saltysd_bootstrap_elf_size);
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
    
    serviceThread(NULL);
    
    free(tids);
}

Result svcQueryProcessMemory(MemoryInfo* meminfo_ptr, u32* pageinfo, Handle debug, u64 addr);

Result handleServiceCmd(int cmd)
{
    Result ret = 0;

    // Send reply
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    if (cmd == 0) // ACK
    {
        ret = 0;
        should_terminate = true;
    }
    else if (cmd == 1)
    {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 command;
            u64 heap;
            u32 reserved[2];
        } *resp = r.Raw;

        Handle proc = r.Handles[0];
        write_log("SaltySD: cmd 1 handler, proc handle %x, heap %llx\n", proc, resp->heap);
        
        u64 new_start;
        ret = load_elf_proc(proc, r.Pid, resp->heap, &new_start, saltysd_core_elf, saltysd_core_elf_size);
        
        /*u64 addr = 0;
        while (1)
        {
            MemoryInfo info;
            u32 pageinfo;
            Result ret = svcQueryProcessMemory(&info, &pageinfo, proc, addr);
            
            write_log("SaltySD: %016llx, size %llx\n", info.addr, info.size);

            addr = info.addr + info.size;
            
            if (!addr || ret) break;
        }*/
        
        svcCloseHandle(proc);
        
        // Ship off results
        struct {
            u64 magic;
            u64 result;
            u64 new_addr;
            u64 reserved[1];
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));

        raw->magic = SFCO_MAGIC;
        raw->result = 0;
        raw->new_addr = new_start;

        return 0;
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
    
    return ret;
}

Handle saltyport;
u8 stack[0x8000];

void serviceThread(void* buf)
{
    Result ret;
    write_log("SaltySD: accepting service calls\n");
    should_terminate = false;

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

                handleServiceCmd(resp->command);

                replySession = session;
                svcSleepThread(1000*1000);
            }
            
            svcCloseHandle(session);
        }

        if (should_terminate) break;
        
        //write_log("SaltySD: ..\n", ret);
        svcSleepThread(1000*1000*100);
    }
    
    write_log("SaltySD: done accepting service calls\n");
    
    //svcCloseHandle(saltyport);
    //svcExitThread();
}

int main(int argc, char *argv[])
{
    Result ret;
    Handle port, fsp, thread;
    
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

    /*ret = svcCreateThread(&thread, serviceThread, NULL, &stack[0x8000],0x31, 3);
    write_log("SaltySD: create thread %x\n", ret);
    ret = svcStartThread(thread);
    write_log("SaltySD: start thread %x\n", ret);*/
    serviceThread(NULL);
    
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

        //write_log("SaltySD: .\n", ret);
        svcSleepThread(1000*1000);
    }
    free(pids);

    return 0;
}

