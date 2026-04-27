#ifndef PROTOCOL_H
#define PROTOCOL_H

/* ─── Konfigurasi The Wired ─────────────────────────────── */
#define WIRED_ADDR      "127.0.0.1"
#define WIRED_PORT      7777
#define PROTOCOL_FILE   "protocol.conf"   /* alamat+port disimpan di sini */
#define LOG_FILE        "history.log"
#define MAX_CLIENTS     32
#define MAX_NAME        64
#define MAX_MSG         1024

/* ─── Identitas khusus The Knights ──────────────────────── */
#define KNIGHTS_NAME    "The Knights"
#define KNIGHTS_PASS    "protocol7"

/* ─── Tipe paket komunikasi ─────────────────────────────── */
#define PKT_REGISTER    1   /* client kirim nama untuk daftar      */
#define PKT_CHAT        2   /* pesan chat biasa                    */
#define PKT_SYSTEM      3   /* notifikasi dari server              */
#define PKT_EXIT        4   /* client minta disconnect             */
#define PKT_RPC_REQ     5   /* The Knights kirim perintah          */
#define PKT_RPC_REPLY   6   /* balasan server ke The Knights       */
#define PKT_AUTH        7   /* hasil autentikasi The Knights       */

/* ─── Paket universal yang dilempar bolak-balik ─────────── */
typedef struct {
    int  type;
    char from[MAX_NAME];
    char body[MAX_MSG];
} Packet;

/* ─── Fungsi utilitas di protocol.c ─────────────────────── */
void write_log(const char *role, const char *content);
int  read_protocol(char *addr, int *port);
void init_protocol(const char *addr, int port);

#endif
