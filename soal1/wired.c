/*
 * wired.c — Server The Wired

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

/* ─── Satu slot di registry = satu jiwa di The Wired ─────── */
typedef struct {
    int  fd;
    char name[MAX_NAME];
    int  is_knights;   /* 1 = admin, 0 = user biasa */
    int  active;       /* 1 = terhubung              */
} Soul;

static Soul            registry[MAX_CLIENTS];
static int             srv_fd   = -1;
static time_t          born_at;                          /* kapan server lahir */
static pthread_mutex_t reg_lock = PTHREAD_MUTEX_INITIALIZER;

/* ════════════════ UTILITAS ════════════════════════════════ */

static int find_by_name(const char *name) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (registry[i].active && strcmp(registry[i].name, name) == 0)
            return i;
    return -1;
}

static int find_by_fd(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (registry[i].active && registry[i].fd == fd)
            return i;
    return -1;
}

static int get_empty_slot(void) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (!registry[i].active) return i;
    return -1;
}

/* Bungkus send() supaya ga perlu repeat setiap kali */
static void push_packet(int fd, int type, const char *from, const char *body) {
    Packet p;
    memset(&p, 0, sizeof(p));
    p.type = type;
    strncpy(p.from, from, MAX_NAME - 1);
    if (body) strncpy(p.body, body, MAX_MSG - 1);
    send(fd, &p, sizeof(p), 0);
}

/* Hapus soul dari registry dan fd_set */
static void evict_soul(int idx, fd_set *master) {
    FD_CLR(registry[idx].fd, master);
    close(registry[idx].fd);
    memset(&registry[idx], 0, sizeof(Soul));
}

/* ════════════════ BROADCAST ══════════════════════════════ */

/* "shout" = teriak ke semua, kecuali si pengirim dan The Knights */
static void shout(Packet *pkt, int skip_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!registry[i].active)        continue;
        if (registry[i].fd == skip_fd)  continue;
        if (registry[i].is_knights)     continue;
        send(registry[i].fd, pkt, sizeof(Packet), 0);
    }
}

/* ════════════════ THE KNIGHTS RPC ══════════════════════ */

static void handle_rpc(int idx, const char *cmd) {
    char reply[MAX_MSG] = {0};

    if (strcmp(cmd, "GET_USERS") == 0) {
        /* Hitung NAVI aktif (Knights tidak dihitung) */
        int  n    = 0;
        char list[MAX_MSG / 2] = "";
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (registry[i].active && !registry[i].is_knights) {
                char line[MAX_NAME + 6];
                snprintf(line, sizeof(line), "  - %s\n", registry[i].name);
                strncat(list, line, sizeof(list) - strlen(list) - 1);
                n++;
            }
        }
        snprintf(reply, MAX_MSG, "Active NAVIs: %d\n%s", n, list);
        write_log("Admin", "RPC_GET_USERS");

    } else if (strcmp(cmd, "GET_UPTIME") == 0) {
        long sec = (long)(time(NULL) - born_at);
        snprintf(reply, MAX_MSG, "Uptime: %02ldh %02ldm %02lds",
                 sec / 3600, (sec % 3600) / 60, sec % 60);
        write_log("Admin", "RPC_GET_UPTIME");

    } else if (strcmp(cmd, "SHUTDOWN") == 0) {
        write_log("Admin", "RPC_SHUTDOWN");
        write_log("System", "EMERGENCY SHUTDOWN INITIATED");

        /* Kirim reply ke Knights dulu */
        snprintf(reply, MAX_MSG, "[System] EMERGENCY SHUTDOWN INITIATED");
        push_packet(registry[idx].fd, PKT_RPC_REPLY, "System", reply);

        /* Broadcast bye ke semua NAVI biasa */
        Packet bye;
        memset(&bye, 0, sizeof(bye));
        bye.type = PKT_SYSTEM;
        strcpy(bye.from, "System");
        strcpy(bye.body, "[System] The Wired is shutting down...");
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (registry[i].active) {
                send(registry[i].fd, &bye, sizeof(bye), 0);
                close(registry[i].fd);
            }
        }
        close(srv_fd);
        printf("[System] EMERGENCY SHUTDOWN INITIATED\n");
        exit(0);

    } else {
        snprintf(reply, MAX_MSG, "Unknown RPC: %s", cmd);
    }

    push_packet(registry[idx].fd, PKT_RPC_REPLY, "System", reply);
}

/* ════════════════ DISCONNECT ════════════════════════════ */

static void disconnect_soul(int idx, fd_set *master) {
    char log_buf[MAX_MSG];
    snprintf(log_buf, sizeof(log_buf), "User '%s' disconnected", registry[idx].name);
    printf("[System] %s\n", log_buf);
    write_log("System", log_buf);

    /* Broadcast disconnection ke yang lain (hanya untuk user biasa) */
    if (!registry[idx].is_knights) {
        Packet pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.type = PKT_SYSTEM;
        strcpy(pkt.from, "System");
        snprintf(pkt.body, MAX_MSG, "[System] User '%s' disconnected",
                 registry[idx].name);
        shout(&pkt, registry[idx].fd);
    }

    evict_soul(idx, master);
}

/* ════════════════ HANDLE PESAN MASUK ════════════════════ */

static void recv_from_soul(int idx, fd_set *master) {
    Packet pkt;
    int    n = recv(registry[idx].fd, &pkt, sizeof(Packet), 0);

    if (n <= 0) {
        /* Koneksi putus tiba-tiba */
        disconnect_soul(idx, master);
        return;
    }

    switch (pkt.type) {

    case PKT_EXIT:
        disconnect_soul(idx, master);
        break;

    case PKT_CHAT: {
        /* Log dan teruskan ke semua */
        char log_entry[MAX_MSG];
        snprintf(log_entry, sizeof(log_entry), "[%s]: %s", pkt.from, pkt.body);
        printf("[User] %s\n", log_entry);
        write_log("User", log_entry);
        shout(&pkt, registry[idx].fd);
        break;
    }

    case PKT_RPC_REQ:
        if (registry[idx].is_knights)
            handle_rpc(idx, pkt.body);
        break;

    default:
        break;
    }
}

/* ════════════════ TERIMA KONEKSI BARU ══════════════════ */

static void accept_soul(fd_set *master, int *fdmax) {
    struct sockaddr_in addr;
    socklen_t          alen = sizeof(addr);

    int new_fd = accept(srv_fd, (struct sockaddr *)&addr, &alen);
    if (new_fd < 0) { perror("accept"); return; }

    /* Terima paket registrasi pertama */
    Packet reg;
    if (recv(new_fd, &reg, sizeof(Packet), 0) <= 0) {
        close(new_fd); return;
    }
    if (reg.type != PKT_REGISTER) {
        close(new_fd); return;
    }

    pthread_mutex_lock(&reg_lock);

    /* ── Cek nama duplikat ── */
    if (find_by_name(reg.from) >= 0) {
        char msg[MAX_MSG];
        snprintf(msg, MAX_MSG,
            "[System] The identity '%s' is already synchronized in The Wired.",
            reg.from);
        push_packet(new_fd, PKT_SYSTEM, "System", msg);
        close(new_fd);
        pthread_mutex_unlock(&reg_lock);
        return;
    }

    /* ── Cek slot kosong ── */
    int slot = get_empty_slot();
    if (slot < 0) {
        push_packet(new_fd, PKT_SYSTEM, "System", "[System] The Wired is full.");
        close(new_fd);
        pthread_mutex_unlock(&reg_lock);
        return;
    }

    int is_knights = (strcmp(reg.from, KNIGHTS_NAME) == 0);

    /* ── Verifikasi password The Knights ── */
    if (is_knights) {
        if (strcmp(reg.body, KNIGHTS_PASS) != 0) {
            push_packet(new_fd, PKT_AUTH, "System", "AUTH_FAIL");
            close(new_fd);
            pthread_mutex_unlock(&reg_lock);
            return;
        }
        push_packet(new_fd, PKT_AUTH, "System", "AUTH_OK");
    }

    /* ── Daftarkan ke registry ── */
    registry[slot].fd         = new_fd;
    registry[slot].is_knights = is_knights;
    registry[slot].active     = 1;
    strncpy(registry[slot].name, reg.from, MAX_NAME - 1);

    FD_SET(new_fd, master);
    if (new_fd > *fdmax) *fdmax = new_fd;

    pthread_mutex_unlock(&reg_lock);

    /* ── Sambut / notifikasi ── */
    if (!is_knights) {
        char welcome[MAX_MSG];
        snprintf(welcome, MAX_MSG, "--- Welcome to The Wired, %s ---", reg.from);
        push_packet(new_fd, PKT_SYSTEM, "System", welcome);

        char log_buf[MAX_MSG];
        snprintf(log_buf, sizeof(log_buf), "User '%s' connected", reg.from);
        printf("[System] %s\n", log_buf);
        write_log("System", log_buf);

        /* Broadcast koneksi baru ke yang lain */
        Packet bc;
        memset(&bc, 0, sizeof(bc));
        bc.type = PKT_SYSTEM;
        strcpy(bc.from, "System");
        snprintf(bc.body, MAX_MSG, "[System] User '%s' connected", reg.from);
        shout(&bc, new_fd);

    } else {
        printf("[System] User 'The Knights' connected\n");
        write_log("System", "User 'The Knights' connected");
    }
}

/* ════════════════ SIGNAL HANDLER ════════════════════════ */

static void sigint_handler(int sig) {
    (void)sig;
    printf("\n[System] Caught SIGINT, shutting down The Wired...\n");
    write_log("System", "EMERGENCY SHUTDOWN INITIATED");

    Packet bye;
    memset(&bye, 0, sizeof(bye));
    bye.type = PKT_SYSTEM;
    strcpy(bye.from, "System");
    strcpy(bye.body, "[System] The Wired is shutting down...");

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (registry[i].active) {
            send(registry[i].fd, &bye, sizeof(bye), 0);
            close(registry[i].fd);
        }
    }
    close(srv_fd);
    exit(0);
}

/* ════════════════ MAIN ══════════════════════════════════ */

int main(void) {
    born_at = time(NULL);
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);
    memset(registry, 0, sizeof(registry));

    /* Tulis konfigurasi ke protocol.conf */
    init_protocol(WIRED_ADDR, WIRED_PORT);

    /* Buat socket server */
    srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port        = htons(WIRED_PORT);

    if (bind(srv_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(srv_fd, 10) < 0) {
        perror("listen"); return 1;
    }

    printf("Orion is ready (PID: %d)\n", getpid());
    write_log("System", "SERVER ONLINE");

    /* ── select() loop utama ── */
    fd_set master, rfds;
    FD_ZERO(&master);
    FD_SET(srv_fd, &master);
    int fdmax = srv_fd;

    while (1) {
        rfds = master;
        if (select(fdmax + 1, &rfds, NULL, NULL, NULL) < 0) {
            perror("select"); break;
        }

        for (int fd = 0; fd <= fdmax; fd++) {
            if (!FD_ISSET(fd, &rfds)) continue;

            if (fd == srv_fd) {
                accept_soul(&master, &fdmax);
            } else {
                int idx = find_by_fd(fd);
                if (idx >= 0) recv_from_soul(idx, &master);
            }
        }
    }

    close(srv_fd);
    return 0;
}
