// 2022117156 박상준

#define BUF_SIZE 1024
#define SENDER 1
#define RECEIBER 2
#define MSG_SEND 1
#define TERMINATE 2

#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>

void error_handling(char *message);
ssize_t read_all(int sock, void *buf, size_t len);
ssize_t write_all(int sock, const void *buf, size_t len);

typedef struct {
    int cmd;
    char buf[BUF_SIZE+1];
} PACKET;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    int sock, str_len;
    struct sockaddr_in serv_addr;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) error_handling("socket() error!\n");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    printf("-------------------------------------------\n");
    printf(" Choose function \n 1: Sender,  2: Receiver\n");
    printf("-------------------------------------------\n");
    printf("=> ");

    int ftn_flag; /* ftn_flag로 SENDER / RECEIBER 분기 */
    scanf("%d", &ftn_flag);

    if (connect(sock, (struct sockaddr*)&serv_addr,
            sizeof(serv_addr)) == -1) {
        error_handling("coonnect() error!");
    }

    /* tcp 연결 수립 완료 */
    printf("Connected...............\n\n");

    if (ftn_flag == SENDER) { // Sender
        FILE *fd2 = fopen("rfc1180.txt", "r");
        if (fd2 == NULL) { /* 에러처리 */
            error_handling("file open error\n");
        }

        printf("File Sender Start!\n");
        /* Sender 마지막 파일 디스크립터는 fileno(fd2) */
        printf("Socket fd: %d, File fd: %d max_fd: %d\n",
                sock, fileno(fd2), fileno(fd2));

        close(fd2);
    } else if (ftn_flag == RECEIBER) {
        printf("File Receiver Start!\n");
        printf("Socket fd: %d, max_fd: %d\n",
                sock, sock);

    } else {
        error_handling("Wrong function number\n");
    }

    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

ssize_t read_all(int sock, void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(sock, (char *)buf + total, len - total);
        if (n == -1) error_handling("read() error");
        if (n == 0) return 0;
        total += n;
    }
    return total;
}

ssize_t write_all(int sock, PACKET *pkt) {
    ssize_t total = 0;
    const char *buf = (const char *)pkt;
    size_t len = sizeof(*pkt);
    while (total < len) {
        ssize_t n = write(sock, buf + total, len - total);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (n == 0)
            return -1;
        total += n;
    }
    return total;
}

void send_file(FILE *fp, int sock) {
    PACKET pkt = {0};
    size_t read_bytes;

    while (1) {
        pkt.cmd = MSG_SEND;

        read_bytes = read_all(fp, pkt.buf, len);
        if (read_bytes == 0) {
            /* 명세에 따라 기능 추가하기 */
            printf("file read end\n");
            break;
        }
        pkt.buf[BUF_SIZE] = '\0';

        size_t send_bytes = write_all(sock, &pkt)

        if (send_bytes != -1) {
            printf("[Tx] File read len: %zd\n", send_bytes);
        } else {
            error_handling("send_file / write_all() error.\n");
        }
        memset(&pkt, 0, sizeof(PACKET));
    }
}
