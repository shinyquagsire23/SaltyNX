#include "ipc_handoffs.h"

#include <switch/kernel/ipc.h>

#include "useful.h"

void ipc_handoffs()
{
    Result ret;
    Handle port;

    ret = svcManageNamedPort(&port, "Shaker", 100);
    write_log("Shaker: svcManageNamedPort returned %x, handle %x\n", ret, port);
    
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
            write_log("session %x being handled\n", session);

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

                write_log("got message from pid %x,\n%s\n", r.Pid, r.Buffers[0]);
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
            
            *(uint32_t*)0xdeadf00f = 0xf00fdead;
        }
        
        
        
        svcSleepThread(0);
    }
    
    svcCloseHandle(port);
}
