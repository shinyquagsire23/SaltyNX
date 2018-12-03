#include "spawner_ipc.h"

#include "useful.h"

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
            *retrieve = r.Handles[0];
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
