#include <switch.h>

#include <string.h>
#include <stdio.h>
#include <switch/kernel/ipc.h>
#include "saltysd_bootstrap_elf.h"
#include "saltysd_core_elf.h"

#include "spawner_ipc.h"

#include "useful.h"

#define MODULE_SALTYSD 420

u32 __nx_applet_type = AppletType_None;

void serviceThread(void* buf);

Handle saltyport, sdcard;
static char g_heap[0x80000];
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
    else if (cmd == 1) // LoadELF
    {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 command;
            u64 heap;
            char name[32];
            u32 reserved[2];
        } *resp = r.Raw;

        Handle proc = r.Handles[0];
        u64 heap = resp->heap;
        write_log("SaltySD: cmd 1 handler, proc handle %x, heap %llx, path %s\n", proc, heap, resp->name);
        
        char* path = malloc(64);
        uint8_t* elf_data = NULL;
        u32 elf_size = 0;

        snprintf(path, 64, "sdmc:/SaltySD/plugins/%s", resp->name);
        FILE* f = fopen(path, "rb");
        if (!f)
        {
            snprintf(path, 64, "sdmc:/SaltySD/%s", resp->name);
            f = fopen(path, "rb");
        }

        if (!f && !strcmp(resp->name, "saltysd_core.elf"))
        {
            write_log("SaltySD: loading builtin %s\n", resp->name);
            elf_data = saltysd_core_elf;
            elf_size = saltysd_core_elf_size;
        }
        else
        {
            fseek(f, 0, SEEK_END);
            elf_size = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            write_log("SaltySD: loading %s, size 0x%x\n", path, elf_size);
            
            elf_data = malloc(elf_size);
            
            fread(elf_data, elf_size, 1, f);
        }
        free(path);
        
        u64 new_start = 0, new_size = 0;
        if (elf_data && elf_size)
            ret = load_elf_proc(proc, r.Pid, heap, &new_start, &new_size, elf_data, elf_size);
        else
            ret = MAKERESULT(MODULE_SALTYSD, 1);

        svcCloseHandle(proc);
        
        if (f)
        {
            free(elf_data);
            fclose(f);
        }
        
        // Ship off results
        struct {
            u64 magic;
            u64 result;
            u64 new_addr;
            u64 new_size;
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));

        raw->magic = SFCO_MAGIC;
        raw->result = ret;
        raw->new_addr = new_start;
        raw->new_size = new_size;
        
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
    else if (cmd == 4) // GetSDCard
    {        
        write_log("SaltySD: cmd 4 handler\n");

        ipcSendHandleCopy(&c, sdcard);
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
    Handle port;
    
    write_log("SaltySD says hello!\n");
    
    do
    {
        ret = svcConnectToNamedPort(&port, "Spawner");
        svcSleepThread(1000*1000);
    }
    while (ret);
    
    // Begin asking for handles
    get_handle(port, &sdcard, "sdcard");
    terminate_spawner(port);
    svcCloseHandle(port);
    
    // Init fs stuff
    FsFileSystem sdcardfs;
    sdcardfs.s.handle = sdcard;
    fsdevMountDevice("sdmc", sdcardfs);

    // Start our port
    // For some reason, we only have one session maximum (0 reslimit handle related?)    
    ret = svcManageNamedPort(&saltyport, "SaltySD", 1);

    // Main service loop
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
    
    fsdevUnmountAll();

    return 0;
}

