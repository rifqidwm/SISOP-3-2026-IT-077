#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <time.h>
#include <string.h>

#define WIRED_ADDR      "127.0.0.1"
#define WIRED_PORT      7777
#define PROTOCOL_FILE   "protocol.conf"
#define LOG_FILE        "history.log"
#define MAX_CLIENTS     32
#define MAX_NAME        64
#define MAX_MSG         1024

#define KNIGHTS_NAME    "The Knights"
#define KNIGHTS_PASS    "protocol7"

#define PKT_REGISTER    1
#define PKT_CHAT        2
#define PKT_SYSTEM      3
#define PKT_EXIT        4
#define PKT_RPC_REQ     5
#define PKT_RPC_REPLY   6
#define PKT_AUTH        7

typedef struct {
    int  type;
    char from[MAX_NAME];
    char body[MAX_MSG];
} Packet;

void write_log(const char *role, const char *content) {
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);
    fprintf(f, "[%s] [%s] [%s]\n", ts, role, content);
    fclose(f);
}

void init_protocol(const char *addr, int port) {
    FILE *f = fopen(PROTOCOL_FILE, "w");
    if (!f) return;
    fprintf(f, "addr=%s\nport=%d\n", addr, port);
    fclose(f);
}

int read_protocol(char *addr, int *port) {
    FILE *f = fopen(PROTOCOL_FILE, "r");
    if (!f) return -1;
    int ok = fscanf(f, "addr=%63s\nport=%d\n", addr, port);
    fclose(f);
    return (ok == 2) ? 0 : -1;
}

#endif
