#include <switch.h>

#include <string.h>
#include <stdio.h>
#include <switch/kernel/ipc.h>

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
    
}

#define write_log(...) \
    {char log_buf[0x200]; snprintf(log_buf, 0x200, __VA_ARGS__); \
    svcOutputDebugString(log_buf, strlen(log_buf));}

int main(int argc, char *argv[])
{
    Result ret;
    Handle port;
    
    write_log("SaltySD says hello!\n");
    
    ret = svcManageNamedPort(&port, "SaltySD", 100);
    write_log("svcManageNamedPort returned %x, handle %x\n", ret, port);
    
    while (1)
    {
        Handle session;
        ret = svcAcceptSession(&session, port);
        if (ret && ret != 0xf201)
        {
            //write_log("svcAcceptSession returned %x\n", ret);
        }
        else if (!ret)
        {
            write_log("SaltySD session %x being handled\n", session);

            int handle_index;
            int reply_num = 0;
            Handle replySession = 0;
            while (1)
            {
                ret = svcReplyAndReceive(&handle_index, &session, 1, replySession, U64_MAX);
                
                //write_log("IPC reply ret %x, index %x, sess %x\n", ret, handle_index, session);
                if (ret) break;
                
                IpcParsedCommand r;
                ipcParse(&r);

                struct {
                    u64 magic;
                    u64 command;
                } *resp = r.Raw;

                write_log("SaltySD got message from pid %x,\n%s\n", r.Pid, r.Buffers[0]);
                //write_log("\n%s\n", resp->command, r.Pid, r.Buffers[0]);
                
                
                // Send reply
                IpcCommand c;
                ipcInitialize(&c);
                ipcSendPid(&c);

                struct {
                    u64 magic;
                    u64 result;
                    char msg[0x10];
                    u64 reserved[1];
                } *raw;

                raw = ipcPrepareHeader(&c, sizeof(*raw));

                raw->magic = SFCO_MAGIC;
                raw->result = 0;
                strncpy(raw->msg, reply_num++ ? "Not much, brb." : "Hey.", 0x10);

                //write_log("sending IPC reply\n");
                //ret = ipcDispatch(session);
                
                replySession = session;
            }
            
            svcCloseHandle(session);
        }

        svcSleepThread(0);
    }
    
    svcCloseHandle(port);

    return 0;
}

