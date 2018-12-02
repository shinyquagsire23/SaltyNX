#include <switch.h>

#include <string.h>
#include <stdio.h>
#include "printf.h"

#define write_log(...) \
    {char log_buf[0x200]; snprintf_(log_buf, 0x200, __VA_ARGS__); \
    svcOutputDebugString(log_buf, _strlen(log_buf));}

void* __saltysd_exit_func = svcExitProcess;

Result _smInit(Handle sm)
{
    Result ret;
    IpcCommand c;

    ipcInitialize(&c);
    ipcSendPid(&c);

    struct 
    {
        u64 magic;
        u64 cmd_id;
        u64 zero;
        u64 reserved[2];
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 0;
    raw->zero = 0;

    ret = ipcDispatch(sm);

    if (R_SUCCEEDED(ret)) 
    {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        ret = resp->result;
    }

    return ret;
}

Result saltySDTerm(Handle salt)
{
    Result ret;
    IpcCommand c;

    ipcInitialize(&c);
    ipcSendPid(&c);

    struct 
    {
        u64 magic;
        u64 cmd_id;
        u64 zero;
        u64 reserved[2];
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 0;
    raw->zero = 0;

    ret = ipcDispatch(salt);

    if (R_SUCCEEDED(ret)) 
    {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        ret = resp->result;
    }

    return ret;
}

Result saltySDLoadELF(Handle salt, u64 heap, u64* elf_addr)
{
    Result ret;
    IpcCommand c;

    ipcInitialize(&c);
    ipcSendPid(&c);
    ipcSendHandleCopy(&c, CUR_PROCESS_HANDLE);

    struct 
    {
        u64 magic;
        u64 cmd_id;
        u64 heap;
        u64 reserved[2];
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 1;
    raw->heap = heap;

    ret = ipcDispatch(salt);

    if (R_SUCCEEDED(ret)) 
    {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
            u64 elf_addr;
        } *resp = r.Raw;

        ret = resp->result;
        *elf_addr = resp->elf_addr;
    }

    return ret;
}

u64 _smEncodeName(const char* name)
{
    u64 name_encoded = 0;
    size_t i;

    for (i=0; i<8; i++)
    {
        if (name[i] == '\0')
            break;

        name_encoded |= ((u64) name[i]) << (8*i);
    }

    return name_encoded;
}

Result _smGetService(Handle* handle_out, Handle sm, const char* name)
{
    IpcCommand c;
    ipcInitialize(&c);
    
    u64 name_encode = _smEncodeName(name);

    struct {
        u64 magic;
        u64 cmd_id;
        u64 service_name;
        u64 reserved[2];
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 1;
    raw->service_name = name_encode;

    Result rc = ipcDispatch(sm);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        rc = resp->result;

        if (R_SUCCEEDED(rc)) {
            *handle_out = r.Handles[0];
        }
    }

    return rc;
}

void*  g_heapAddr;
size_t g_heapSize;

void setupAppHeap(void)
{
    u64 size = 0;
    void* addr = NULL;
    Result rc = 0;

    rc = svcSetHeapSize(&addr, 0x200000);

    if (rc || addr == NULL)
    {
        write_log("SaltySD Bootstrap: svcSetHeapSize failed with err %x\n", rc);
    }

    g_heapAddr = addr;
    g_heapSize = 0x200000;
}

int main(int argc, char *argv[])
{
    Result ret;
    Handle sm, saltysd;

    write_log("SaltySD Bootstrap: we in here\n");
    
    setupAppHeap();
    
    ret = svcConnectToNamedPort(&sm, "sm:");
    if (ret) goto fail;

    ret = _smGetService(&saltysd, sm, "SaltySD");
    if (ret)
    {
        ret = _smInit(sm);
        if (ret) goto fail;
        
        ret = _smGetService(&saltysd, sm, "SaltySD");
        if (ret) goto fail;
    }

    write_log("SaltySD Bootstrap: Got handle %x, loading ELF...\n", saltysd);
    u64 new_addr;
    ret = saltySDLoadELF(saltysd, g_heapAddr, &new_addr);
    if (ret) goto fail;
    
    write_log("SaltySD Bootstrap: ELF loaded to %p\n", (void*)new_addr);
    __saltysd_exit_func = new_addr;

    svcCloseHandle(saltysd);
    svcCloseHandle(sm);

    return 0;

fail:
    write_log("SaltySD Bootstrap: failed with retcode %x\n", ret);
    return 0;
}

