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
		char name[13];
		u32 reserved;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = 1;
	strncpy(raw->name, name, 12);

	debug_log("SaltySD: sending IPC request for handle %s\n", name);
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
		//debug_log("SaltySD: Got reply %x\n", resp->result);
		
		if (!ret)
		{
			debug_log("SaltySD: bind handle %x to %s\n", r.Handles[0], name);
			smAddOverrideHandle(smEncodeName(name), r.Handles[0]);
			*retrieve = r.Handles[0];
		}
	}
	else
	{
		//debug_log("SaltySD: IPC dispatch failed, %x\n", ret);
	}
	
	return ret;
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

	debug_log("SaltySD: terminating spawner\n");
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
