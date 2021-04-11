#include "saltysd_ipc.h"

#include <switch.h>
#include <stdarg.h>
#include "ipc.h"

#include "saltysd_core.h"
#include "useful.h"

Handle saltysd;

Result SaltySD_Term()
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

	ret = ipcDispatch(saltysd);

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
	
	// Session terminated works too.
	if (ret == 0xf601) return 0;

	return ret;
}

Result SaltySD_Restore()
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

	ret = ipcDispatch(saltysd);

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

Result SaltySD_LoadELF(u64 heap, u64* elf_addr, u64* elf_size, char* name)
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
		char name[64];
		u64 reserved[2];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = 1;
	raw->heap = heap;
	strncpy(raw->name, name, 63);

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) 
	{
		IpcParsedCommand r;
		ipcParse(&r);

		struct {
			u64 magic;
			u64 result;
			u64 elf_addr;
			u64 elf_size;
		} *resp = r.Raw;

		ret = resp->result;
		*elf_addr = resp->elf_addr;
		*elf_size = resp->elf_size;
	}

	return ret;
}

Result SaltySD_Memcpy(u64 to, u64 from, u64 size)
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

	ret = ipcDispatch(saltysd);

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

Result SaltySD_Exception()
{
	Result ret;
	IpcCommand c;

	ipcInitialize(&c);
	ipcSendPid(&c);

	struct 
	{
		u64 magic;
		u64 cmd_id;
		u32 reserved[4];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = 9;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) 
	{
		IpcParsedCommand r;
		ipcParse(&r);

		struct {
			u64 magic;
			u64 result;
			u64 reserved[2];
		} *resp = r.Raw;

		ret = resp->result;
	}

	return ret;
}

Result SaltySD_GetSDCard(Handle *retrieve)
{
	Result ret = 0;

	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	struct {
		u64 magic;
		u64 cmd_id;
		u32 reserved[4];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = 4;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct {
			u64 magic;
			u64 result;
			u64 reserved[2];
		} *resp = r.Raw;

		ret = resp->result;
		
		if (!ret)
		{
			SaltySDCore_printf("SaltySD Core: got SD card handle %x\n", r.Handles[0]);
			*retrieve = r.Handles[0];
			
			// Init fs stuff
			FsFileSystem sdcardfs;
			sdcardfs.s.session = *retrieve;
			sdcardfs.s.own_handle = 0;
			sdcardfs.s.pointer_buffer_size = 0x800;
			int dev = fsdevMountDevice("sdmc", sdcardfs);
			setDefaultDevice(dev);
			SaltySDCore_printf("Confirmation");
		}
	}
	
	return ret;
}

Result SaltySD_print(char* out)
{
	Result ret;
	IpcCommand c;

	ipcInitialize(&c);

	struct 
	{
		u64 magic;
		u64 cmd_id;
		char log[65];
		u64 reserved[2];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = 5;
	strncpy(raw->log, out, 64);

	ret = ipcDispatch(saltysd);

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

Result SaltySD_printf(const char* format, ...)
{
	Result ret;
	char tmp[256];

	va_list args;
	va_start(args, format);
	vsnprintf(tmp, 256, format, args);
	va_end(args);
	
	int i = 0;
	while(i < strlen(tmp)) {
		ret = SaltySD_print(tmp + i);
		i += 64;

		if (ret) {
			i = 0;
			break;
		}
	}

	if (ret)
	{
		debug_log("SaltySD Core: failed w/ error %x, msg: %s", ret, tmp);
	}

	return ret;
}


