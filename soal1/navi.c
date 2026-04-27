#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

static int sock_fd = -1;
static volatile sig_atomic_t running = 1;
static char my_name[MAX_NAME];

/* Hapus newline kalau ada */
static void trim_newline(char *s) {
    if (!s) return;
    s[strcspn(s, "\n")] = '\0';
}

/* Prompt kecil biar alurnya lebih enak dibaca */
static void show_prompt(void) {
    printf("> ");
    fflush(stdout);
}

/* Bungkus send ─ tetap dipakai biar rapi */
static void push_packet(int type, const char *from, const char *body) {
    Packet p;
    memset(&p, 0, sizeof(p));

    p.type = type;
    strncpy(p.from, from, MAX_NAME - 1);
    if (body) {
        strncpy(p.body, body, MAX_MSG - 1);
    }

    if (send(sock_fd, &p, sizeof(p), 0) < 0) {
        perror("[System] send");
    }
}

/* ════════════════ THREAD 1: DENGARKAN SERVER ═══════════ */

static void *recv_thread(void *arg) {
    (void)arg;
    Packet pkt;

    while (running) {
        int n = recv(sock_fd, &pkt, sizeof(Packet), 0);
        if (n <= 0) {
            printf("\n[System] Disconnected from The Wired.\n");
            running = 0;
            break;
        }

        if (pkt.type == PKT_SYSTEM) {
            printf("\n%s\n", pkt.body);
            show_prompt();
        } else if (pkt.type == PKT_CHAT) {
            printf("\n[%s]: %s\n", pkt.from, pkt.body);
            show_prompt();
        } else if (pkt.type == PKT_RPC_REPLY) {
            printf("\n%s\n", pkt.body);
            show_prompt();
        } else {
            /* Kadang paket aneh bisa lewat, ya udah diam aja */
        }
    }

    return NULL;
}

/* ════════════════ THREAD 2: KIRIM INPUT USER ══════════ */

static void *send_thread(void *arg) {
    (void)arg;
    char buf[MAX_MSG];

    while (running) {
        show_prompt();

        if (!fgets(buf, sizeof(buf), stdin)) {
            running = 0;
            break;
        }

        trim_newline(buf);

        if (!running) break;

        if (strcmp(buf, "/exit") == 0) {
            push_packet(PKT_EXIT, my_name, "");
            printf("[System] Disconnecting from The Wired...\n");
            running = 0;
            break;
        }

        if (strlen(buf) == 0) {
            continue;
        }

        if (strlen(buf) > MAX_MSG - 2) {
            printf("[System] Pesan terlalu panjang, dipotong dulu ya.\n");
        }

        push_packet(PKT_CHAT, my_name, buf);
    }

    return NULL;
}

/* ════════════════ MODE THE KNIGHTS (SINKRON) ══════════ */

static void knights_console(void) {
    Packet pkt;
    char choice[8];

    printf("\n[System] Authentication Successful. Granted Admin privileges.\n");

    while (1) {
        printf("\n=== THE KNIGHTS CONSOLE ===\n");
        printf("1. Check Active Entities (Users)\n");
        printf("2. Check Server Uptime\n");
        printf("3. Execute Emergency Shutdown\n");
        printf("4. Disconnect\n");
        printf("Command >> ");
        fflush(stdout);

        if (!fgets(choice, sizeof(choice), stdin)) break;
        trim_newline(choice);

        const char *cmd = NULL;
        if (strcmp(choice, "1") == 0) {
            cmd = "GET_USERS";
        } else if (strcmp(choice, "2") == 0) {
            cmd = "GET_UPTIME";
        } else if (strcmp(choice, "3") == 0) {
            cmd = "SHUTDOWN";
        } else if (strcmp(choice, "4") == 0) {
            push_packet(PKT_EXIT, my_name, "");
            printf("[System] Disconnecting from The Wired...\n");
            return;
        } else {
            printf("[!] Pilihan tidak valid.\n");
            continue;
        }

        push_packet(PKT_RPC_REQ, my_name, cmd);

        /* Tunggu balasan dari server */
        int n = recv(sock_fd, &pkt, sizeof(Packet), 0);
        if (n <= 0) {
            printf("[System] Server disconnected.\n");
            return;
        }

        if (pkt.type == PKT_RPC_REPLY || pkt.type == PKT_SYSTEM) {
            printf("\n%s\n", pkt.body);
        } else {
            printf("\n[System] Unexpected reply from server.\n");
        }

        /* Kalau SHUTDOWN, server langsung mati */
        if (strcmp(cmd, "SHUTDOWN") == 0) {
            return;
        }
    }
}

/* ════════════════ MAIN ════════════════════════════════ */

int main(void) {
    signal(SIGPIPE, SIG_IGN);

    /* Baca konfigurasi dari protocol.conf */
    char addr[64];
    int port = WIRED_PORT;

    strncpy(addr, WIRED_ADDR, sizeof(addr) - 1);
    addr[sizeof(addr) - 1] = '\0';

    if (read_protocol(addr, &port) < 0) {
        printf("[!] protocol.conf tidak ditemukan, pakai default %s:%d\n", addr, port);
    }

    /*
     * Loop registrasi:
     * kalau nama ditolak (duplikat / penuh),
     * client coba lagi dengan nama baru.
     */
    while (1) {
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            perror("socket");
            return 1;
        }

        struct sockaddr_in serv;
        memset(&serv, 0, sizeof(serv));
        serv.sin_family = AF_INET;
        serv.sin_port = htons(port);

        if (inet_pton(AF_INET, addr, &serv.sin_addr) <= 0) {
            printf("[!] Alamat server tidak valid.\n");
            close(sock_fd);
            return 1;
        }

        if (connect(sock_fd, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
            perror("[!] Cannot connect to The Wired");
            close(sock_fd);
            return 1;
        }

        printf("Enter your name: ");
        fflush(stdout);

        if (!fgets(my_name, sizeof(my_name), stdin)) {
            close(sock_fd);
            return 1;
        }
        trim_newline(my_name);

        if (strlen(my_name) == 0) {
            close(sock_fd);
            continue;
        }

        int is_knights = (strcmp(my_name, KNIGHTS_NAME) == 0);
        char password[MAX_MSG] = "";

        if (is_knights) {
            printf("Enter Password: ");
            fflush(stdout);

            if (!fgets(password, sizeof(password), stdin)) {
                close(sock_fd);
                return 1;
            }
            trim_newline(password);
        }

        /* Kirim paket registrasi */
        Packet reg;
        memset(&reg, 0, sizeof(reg));
        reg.type = PKT_REGISTER;
        strncpy(reg.from, my_name, MAX_NAME - 1);

        if (is_knights) {
            strncpy(reg.body, password, MAX_MSG - 1);
        }

        if (send(sock_fd, &reg, sizeof(reg), 0) < 0) {
            perror("[System] send register");
            close(sock_fd);
            return 1;
        }

        /* Terima respons pertama */
        Packet resp;
        memset(&resp, 0, sizeof(resp));

        if (recv(sock_fd, &resp, sizeof(Packet), 0) <= 0) {
            printf("[System] No response from The Wired.\n");
            close(sock_fd);
            return 1;
        }

        /* ── The Knights: cek auth ── */
        if (is_knights) {
            if (resp.type == PKT_AUTH) {
                if (strcmp(resp.body, "AUTH_FAIL") == 0) {
                    printf("[System] Authentication Failed.\n");
                    close(sock_fd);
                    return 1;
                }

                knights_console();
                close(sock_fd);
                return 0;
            }

            printf("[System] Unexpected auth response.\n");
            close(sock_fd);
            return 1;
        }

        /* ── User biasa: cek welcome vs rejection ── */
        if (resp.type == PKT_SYSTEM) {
            printf("%s\n", resp.body);

            /* Kalau ditolak, coba lagi */
            if (strstr(resp.body, "already synchronized") ||
                strstr(resp.body, "full")) {
                printf("[System] Nama itu belum bisa dipakai. Coba lagi ya.\n");
                close(sock_fd);
                sleep(1);
                continue;
            }

            break;
        }

        /* Kalau responnya bukan yang diharapkan, keluar aja */
        printf("[System] Unexpected response from server.\n");
        close(sock_fd);
        return 1;
    }

    /* ── Mode user biasa: 2 thread async ── */
    pthread_t tid_recv, tid_send;

    if (pthread_create(&tid_recv, NULL, recv_thread, NULL) != 0) {
        perror("pthread_create recv_thread");
        close(sock_fd);
        return 1;
    }

    if (pthread_create(&tid_send, NULL, send_thread, NULL) != 0) {
        perror("pthread_create send_thread");
        running = 0;
        pthread_cancel(tid_recv);
        pthread_join(tid_recv, NULL);
        close(sock_fd);
        return 1;
    }

    /* Tunggu send_thread selesai (user /exit) */
    pthread_join(tid_send, NULL);

    /* Hentikan recv_thread kalau masih nunggu recv() */
    running = 0;
    pthread_cancel(tid_recv);
    pthread_join(tid_recv, NULL);

    close(sock_fd);
    return 0;
}
