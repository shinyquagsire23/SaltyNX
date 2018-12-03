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

Handle saltyport;
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

    if (cmd == 0) // EndSession
    {
        ret = 0;
        should_terminate = true;
        write_log("SaltySD: cmd 0, terminating\n");
    }
    else if (cmd == 1) // LoadCore
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
        raw->result = ret;
        raw->new_addr = new_start;
        
        write_log("SaltySD: new_addr to %llx, %x\n", new_start, ret);

        return 0;
    }
    else if (cmd == 2) // RestoreBootstrapCode
    {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 command;
            u32 reserved[4];
        } *resp = r.Raw;

        write_log("SaltySD: cmd 2 handler\n");
        
        Handle debug;
        ret = svcDebugActiveProcess(&debug, r.Pid);
        if (!ret)
        {
            ret = restore_elf_debug(debug);
        }

        svcCloseHandle(debug);
    }
    else if (cmd == 3) // Memcpy
    {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 command;
            u64 to;
            u64 from;
            u64 size;
        } *resp = r.Raw;

        write_log("SaltySD: cmd 3 handler, memcpy(%llx, %llx, %llx)\n", resp->to, resp->from, resp->size);
        
        Handle debug;
        ret = svcDebugActiveProcess(&debug, r.Pid);
        if (!ret)
        {
            u8* tmp = malloc(resp->size);

            ret = svcReadDebugProcessMemory(tmp, debug, resp->from, resp->size);
            ret = svcWriteDebugProcessMemory(debug, tmp, resp->to, resp->size);

            free(tmp);
            
            ret = svcCloseHandle(debug);
        }
        
        // Ship off results
        struct {
            u64 magic;
            u64 result;
            u64 reserved[2];
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));

        raw->magic = SFCO_MAGIC;
        raw->result = 0;

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
                
                if (should_terminate) break;

                replySession = session;
                svcSleepThread(1000*1000);
            }
            
            svcCloseHandle(session);
        }

        if (should_terminate) break;
        
        svcSleepThread(1000*1000*100);
    }
    
    write_log("SaltySD: done accepting service calls\n");
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
    terminate_spawner(port);
    svcCloseHandle(port);

    // Start our port
    // For some reason, we only have one session maximum (0 reslimit handle related?)    
    ret = svcManageNamedPort(&saltyport, "SaltySD", 1);

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
        
        // If someone is waiting for us, handle them.
        if (!svcWaitSynchronizationSingle(saltyport, 1000))
        {
            serviceThread(NULL);
        }

        svcSleepThread(1000*1000);
    }
    free(pids);

    return 0;
}

