#include <switch.h>

#include <string.h>
#include <stdio.h>

#define write_log(...) \
    {char log_buf[0x200]; snprintf(log_buf, 0x200, __VA_ARGS__); \
    svcOutputDebugString(log_buf, strlen(log_buf));}

int main(int argc, char *argv[])
{
    write_log("SaltySD Bootstrap: we in here\n");

    return 0;
}

