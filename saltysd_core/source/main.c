#include <switch.h>

#include <string.h>
#include <stdio.h>
#include <switch/kernel/ipc.h>

#include "useful.h"

u32 __nx_applet_type = AppletType_None;

static char g_heap[0x20000];

extern void _start();
extern void __nx_exit_clear(void* ctx, Handle thread, void* addr);

Handle orig_main_thread;
void* orig_ctx;

void __libnx_init(void* ctx, Handle main_thread, void* saved_lr)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];
    
    orig_ctx = ctx;
    orig_main_thread = main_thread;
    
    // Call constructors.
    void __libc_init_array(void);
    __libc_init_array();
}

u64 getCodeStart()
{
    u64 addr = 0;
    while (1)
    {
        MemoryInfo info;
        u32 pageinfo;
        Result ret = svcQueryMemory(&info, &pageinfo, addr);
        
        if (info.addr != (u64)_start && info.perm == Perm_Rx)
        {
            addr = info.addr;
            break;
        }

        addr = info.addr + info.size;

        if (!addr || ret) break;
    }
    return addr;
}

u64 findCode(u8* code, size_t size)
{
    u64 addr = getCodeStart();

    while (1)
    {
        if (!memcmp((void*)addr, code, size)) return addr;

        MemoryInfo info;
        u32 pageinfo;
        Result ret = svcQueryMemory(&info, &pageinfo, addr);
        
        if (info.perm != Perm_Rx)
        {
            addr = info.addr + info.size;
        }
        else
        {
            addr += sizeof(u32);
        }
        if (ret) break;
    }

    return 0;
}

u64 getCodeSize()
{
    u64 addr = 0;
    while (1)
    {
        MemoryInfo info;
        u32 pageinfo;
        Result ret = svcQueryMemory(&info, &pageinfo, addr);
        
        if (info.addr != (u64)_start && info.perm == Perm_Rx)
        {
            return info.size;
        }

        addr = info.addr + info.size;

        if (!addr || ret) break;
    }

    return 0;
}

void __attribute__((weak)) NORETURN __libnx_exit(int rc)
{
    // Call destructors.
    void __libc_fini_array(void);
    __libc_fini_array();
    
    u64 addr = getCodeStart();

    write_log("SaltySD Core: jumping to %llx\n", addr);

    __nx_exit_clear(orig_ctx, orig_main_thread, (void*)addr);
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

Result saltySDRestore(Handle salt)
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
    raw->cmd_id = 2;
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
    
    armICacheInvalidate(getCodeStart(), getCodeSize());
    

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

Result saltySDMemcpy(Handle salt, u64 to, u64 from, u64 size)
{
    Result ret;
    IpcCommand c;

    ipcInitialize(&c);
    ipcSendPid(&c);

    struct 
    {
        u64 magic;
        u64 cmd_id;
        u64 to;
        u64 from;
        u64 size;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 3;
    raw->from = from;
    raw->to = to;
    raw->size = size;

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

Result svcSetHeapSizeIntercept(u64 *out, u64 size)
{
    Result ret = svcSetHeapSize(out, size+0x200000);
    
    //write_log("SaltySD Core: svcSetHeapSize intercept %x %llx %llx\n", ret, *out, size+0x200000);
    
    if (!ret)
    {
        *out += 0x200000;
    }
    
    return ret;
}

Result svcGetInfoIntercept (u64 *out, u64 id0, Handle handle, u64 id1)
{
    Result ret = svcGetInfo(out, id0, handle, id1);
    
    //write_log("SaltySD Core: svcGetInfo intercept %p (%llx) %llx %x %llx ret %x\n", out, *out, id0, handle, id1, ret);
    
    return ret;
}

int main(int argc, char *argv[])
{
    Result ret;
    Handle sm, saltysd;
    
    write_log("SaltySD Core: waddup\n");
    
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

    write_log("SaltySD Core: Got handle %x, restoring code...\n", saltysd);
    ret = saltySDRestore(saltysd);
    if (ret) goto fail;

    u8 orig_1[0x8] = {0xE0, 0x0F, 0x1F, 0xF8, 0x21, 0x00, 0x00, 0xD4};
    u8 orig_2[0x8] = {0xE0, 0x0F, 0x1F, 0xF8, 0x21, 0x05, 0x00, 0xD4};
    u8 nop[0x4] = {0x1F, 0x20, 0x03, 0xD5};
    u8 patch[0x10] = {0x44, 0x00, 0x00, 0x58, 0x80, 0x00, 0x1F, 0xD6, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x00};
    
    u64 code = getCodeStart();
    u64 dst_1 = findCode(orig_1, 8);
    u64 dst_2 = findCode(orig_2, 8);
    
    write_log("found code at %llx, %llx\n", dst_1, dst_2);

    *(u64*)&patch[8] = svcSetHeapSizeIntercept;
    if (dst_1 & 4)
    {
        saltySDMemcpy(saltysd, dst_1, (u64)nop, 0x4);
        saltySDMemcpy(saltysd, dst_1+4, (u64)patch, 0x10);
    }
    else
    {
        saltySDMemcpy(saltysd, dst_1, (u64)patch, 0x10);
    }
    
    *(u64*)&patch[8] = svcGetInfoIntercept;
    if (dst_2 & 4)
    {
        saltySDMemcpy(saltysd, dst_2, (u64)nop, 0x4);
        saltySDMemcpy(saltysd, dst_2+4, (u64)patch, 0x10);
    }
    else
    {
        saltySDMemcpy(saltysd, dst_2, (u64)patch, 0x10);
    }
    
    u32 zero;
    //saltySDMemcpy(saltysd, code + 0x1E982C + 0x4000, &zero, 4);

    write_log("SaltySD Core: terminating\n");
    ret = saltySDTerm(saltysd);
    if (ret) goto fail;

    svcCloseHandle(saltysd);
    svcCloseHandle(sm);

    __libnx_exit(0);

fail:
    write_log("SaltySD Core: failed with retcode %x\n", ret);
    __libnx_exit(0);
}

