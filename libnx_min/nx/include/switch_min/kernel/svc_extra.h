/**
 * @file svc.h
 * @brief Extra wrappers for kernel syscalls.
 * @copyright libnx Authors
 */
#pragma once
#include "../types.h"

typedef struct {
    u32 type;
    u32 flags;
    u64 threadId;
    union
    {
        // AttachProcess
        struct
        {
            u64 tid;
            u64 pid;
            char name[12];
            u32 mmuFlags;
            u64 userExceptionContextAddr;
        };
        
        // AttachThread
        struct
        {
           u64 attachThreadId;
           u64 tlsPtr;
           u64 entrypoint; 
        };
        
        // ExitProcess/ExitThread
        struct
        {
            u32 exitType;
        };
        
        // Exception
        struct
        {
            u32 exceptionType;
            u32 faultRegister;
        };
    };
} DebugEventInfo;

/// DebugEventInfo types
typedef enum {
    DebugEvent_AttachProcess=0, ///< Attached to process
    DebugEvent_AttachThread=1,  ///< Attached to thread
    DebugEvent_ExitProcess=2,   ///< Exited process
    DebugEvent_ExitThread=3,    ///< Exited thread
    DebugEvent_Exception=4,     ///< Exception
} DebugEvent;

/**
 * @brief Gets an incoming debug event from a debugging session.
 * @return Result code.
 * @note Syscall number 0x63.
 * @warning This is a privileged syscall. Use \ref envIsSyscallHinted to check if it is available.
 */
inline Result svcGetDebugEventInfo(DebugEventInfo* event_out, Handle debug)
{
    return svcGetDebugEvent((u8*)event_out, debug);
}
