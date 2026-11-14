// 2022117156 박상준

#define BUF_SIZE 1024
#define SENDER 1
#define RECEIVER 2
#define MSG_SEND 1
#define TERMINATE 2

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#pragma pack(push, 1)
typedef struct {
    int cmd;
    char buf[BUF_SIZE + 1];
} PACKET;
#pragma pack(pop)

static ssize_t read_all(int sock, void *buf, size_t len) {
    size_t total = 0;

    while (total < len) {
        ssize_t n = read(sock, (char *)buf + total, len - total);

        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;  /* 정상 EOF. 부분 수신이면 total 반환 */

        total += (size_t)n;
    }

    return (ssize_t)total;
}

static ssize_t write_all(int sock, const void *buf, size_t len) {
    size_t total = 0;

    while (total < len) {
        ssize_t n = write(sock, (const char *)buf + total, len - total);

        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            errno = EIO;
            return -1;
        }

        total += (size_t)n;
    }

    return (ssize_t)total;
}

/* Sender: fd2(파일)를 select()로 감시하고, 1초 간격으로 전송한다. */
static int send_file(FILE *fp, int sock) {
    int file_fd = fileno(fp);
    if (file_fd < 0 || file_fd >= FD_SETSIZE) {
        fprintf(stderr, "Invalid file descriptor for select()\n");
        return -1;
    }

    fd_set reads, cpy_reads;
    FD_ZERO(&reads);
    FD_SET(file_fd, &reads);

    printf("File Sender Start!\n");
    printf("Socket fd: %d, File fd: %d max_fd: %d\n",
           sock, file_fd, file_fd);

    while (1) {
        cpy_reads = reads;
        struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };

        int ready = select(file_fd + 1, &cpy_reads, NULL, NULL, &timeout);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select(sender)");
            return -1;
        }

        if (ready == 0) continue;

        if (!FD_ISSET(file_fd, &cpy_reads)) continue;

        PACKET pkt = {0};
        pkt.cmd = MSG_SEND;

        size_t read_bytes = fread(pkt.buf, 1, BUF_SIZE, fp);
        if (ferror(fp)) {
            perror("fread(rfc1180.txt)");
            FD_CLR(file_fd, &reads);
            return -1;
        }

        if (read_bytes > 0) {
            /* 마지막 짧은 청크에도 NULL 추가 */
            pkt.buf[read_bytes] = '\0';

            if (write_all(sock, &pkt, sizeof(pkt)) != (ssize_t)sizeof(pkt)) {
                perror("write_all(sender)");
                FD_CLR(file_fd, &reads);
                return -1;
            }

            /* 전송 패킷 크기가 아닌, 실제 파일에서 읽은 크기 */
            printf("[Tx] File read len: %zu\n", read_bytes);
            fflush(stdout);
            sleep(1);  /* 1초 지연 전송 */
        }

        if (read_bytes < BUF_SIZE) {
            FD_CLR(file_fd, &reads);
            printf("file read end\n");
            return 0;
        }
    }
}

/* Receiver: fd1(소켓)을 select()로 감시한다. */
static int receive_file(int sock) {
    if (sock >= FD_SETSIZE) {
        fprintf(stderr, "Socket descriptor exceeds FD_SETSIZE\n");
        return -1;
    }

    fd_set reads, cpy_reads;
    FD_ZERO(&reads);
    FD_SET(sock, &reads);

    printf("File Receiver Start!\n");
    printf("Socket fd: %d, max_fd: %d\n", sock, sock);

    while (1) {
        cpy_reads = reads;
        struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };

        int ready = select(sock + 1, &cpy_reads, NULL, NULL, &timeout);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            perror("select(receiver)");
            FD_CLR(sock, &reads);
            return -1;
        }
        if (ready == 0)
            continue;
        if (!FD_ISSET(sock, &cpy_reads))
            continue;

        PACKET pkt;
        ssize_t n = read_all(sock, &pkt, sizeof(pkt));
        if (n < 0) {
            perror("read_all(receiver)");
            FD_CLR(sock, &reads);
            return -1;
        }
        if (n == 0) {
            fprintf(stderr, "Server closed without TERMINATE\n");
            FD_CLR(sock, &reads);
            return -1;
        }
        if (n != (ssize_t)sizeof(pkt)) {
            fprintf(stderr, "Incomplete packet: %zd / %zu bytes\n",
                    n, sizeof(pkt));
            FD_CLR(sock, &reads);
            return -1;
        }

        if (pkt.cmd == MSG_SEND) {
            /* printf("%s")가 패킷 밖으로 읽지 않도록 방어 */
            pkt.buf[BUF_SIZE] = '\0';
            printf("%s", pkt.buf);
            fflush(stdout);
        } else if (pkt.cmd == TERMINATE) {
            FD_CLR(sock, &reads);
            return 0;
        } else {
            fprintf(stderr, "Unknown packet cmd: %d\n", pkt.cmd);
            FD_CLR(sock, &reads);
            return -1;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *end = NULL;
    errno = 0;
    long port = strtol(argv[2], &end, 10);
    if (errno != 0 || end == argv[2] || *end != '\0' ||
        port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal(SIGPIPE)");
        return EXIT_FAILURE;
    }

    printf("-------------------------------------------\n");
    printf(" Choose function \n 1: Sender,  2: Receiver\n");
    printf("-------------------------------------------\n");
    printf("=> ");
    fflush(stdout);

    int role;
    if (scanf("%d", &role) != 1 ||
        (role != SENDER && role != RECEIVER)) {
        fprintf(stderr, "Wrong function number\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int status = EXIT_SUCCESS;
    FILE *fp = NULL;

    if (connect(sock, (struct sockaddr *)&serv_addr,
                sizeof(serv_addr)) == -1) {
        perror("connect");
        status = EXIT_FAILURE;
        goto cleanup;
    }
    printf("Connected...............\n\n");

    if (role == SENDER) {
        fp = fopen("rfc1180.txt", "rb");
        if (fp == NULL) {
            perror("fopen(rfc1180.txt)");
            status = EXIT_FAILURE;
            goto cleanup;
        }

        if (send_file(fp, sock) != 0)
            status = EXIT_FAILURE;
    } else {
        if (receive_file(sock) != 0)
            status = EXIT_FAILURE;
    }

cleanup:
    if (fp && fclose(fp) == EOF) {
        perror("fclose(rfc1180.txt)");
        status = EXIT_FAILURE;
    }
    if (close(sock) == -1) {
        perror("close(sock)");
        status = EXIT_FAILURE;
    }
    return status;
}

