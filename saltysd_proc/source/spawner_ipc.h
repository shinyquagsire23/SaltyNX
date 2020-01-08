#ifndef SPAWNER_IPC_H
#define SPAWNER_IPC_H

#include <switch_min.h>

Result get_handle(Handle port, Handle *retrieve, char* name);
void get_port(Handle port, Handle *retrieve, char* name);
void terminate_spawner(Handle port);

#endif // SPAWNER_IPC_H
