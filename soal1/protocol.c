#include <stdio.h>
#include <time.h>
#include <string.h>
#include "protocol.h"

/*
 * write_log — catat semua kejadian ke history.log
 * Format: [YYYY-MM-DD HH:MM:SS] [role] [content]
 */
void write_log(const char *role, const char *content) {
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) return;

    time_t      now = time(NULL);
    struct tm  *t   = localtime(&now);
    char        ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);

    fprintf(f, "[%s] [%s] [%s]\n", ts, role, content);
    fclose(f);
}

/*
 * init_protocol — server tulis alamat+port ke file
 * supaya NAVI (client) bisa baca tanpa hardcode
 */
void init_protocol(const char *addr, int port) {
    FILE *f = fopen(PROTOCOL_FILE, "w");
    if (!f) return;
    fprintf(f, "addr=%s\nport=%d\n", addr, port);
    fclose(f);
}

/*
 * read_protocol — NAVI baca konfigurasi dari file
 * return 0 kalau berhasil, -1 kalau file tidak ada
 */
int read_protocol(char *addr, int *port) {
    FILE *f = fopen(PROTOCOL_FILE, "r");
    if (!f) return -1;
    int ok = fscanf(f, "addr=%63s\nport=%d\n", addr, port);
    fclose(f);
    return (ok == 2) ? 0 : -1;
}
