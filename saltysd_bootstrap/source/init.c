#include <switch.h>
#include <sys/iosupport.h>

int __system_argc;
char** __system_argv;
void* __stack_top;
void NORETURN __nx_exit(Result rc, LoaderReturnFn retaddr);

__attribute__((weak)) void* __saltysd_exit_func = svcExitProcess;

void __attribute__((weak)) __rel_init(void* ctx, Handle main_thread, void* saved_lr)
{
    __system_argc = 0;
    __system_argv = NULL;
}

void __attribute__((weak)) NORETURN __rel_exit(int rc)
{
    __nx_exit(0, __saltysd_exit_func);
}
