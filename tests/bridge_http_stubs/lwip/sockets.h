#pragma once
#define IPPROTO_TCP 6
#define TCP_NODELAY 1
#define setsockopt fake_setsockopt
int fake_setsockopt(int socket, int level, int option, const void *value, unsigned length);
