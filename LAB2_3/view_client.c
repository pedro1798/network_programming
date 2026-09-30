/* 2022117156 박상준 */

#define BUF_SIZE 100
// cmd type
#define FILE_REQ 1 // clnt
#define FILE_RES 2 // serv
#define FILE_END 3 // serv
#define FILE_END_ACK 4 // clnt
#define FILE_NOT_FOUND 5 // serv

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

void error_handling(char *message);

typedef struct {
    int cmd;
    int buf_len;
    char buf[BUF_SIZE+1]; // null 자리
} PACKET;

ssize_t read_all(int sock, void *buf, size_t len);
ssize_t write_all(int sock, const void *buf, size_t len);

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    int str_len;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock,
                (struct sockaddr*)&serv_addr,
                sizeof(serv_addr)) == -1) {
        error_handling("connect() error!");
    }

    /* ----------- ^^boilerplate code^^ ----------- */

    printf("Input file name: ");

    PACKET clnt_pkt;
    memset(&clnt_pkt, 0, sizeof(PACKET));

    if (scanf("%100s", clnt_pkt.buf) == -1) {
        error_handling("scanf() error");
    }
    getchar(); /* 파일 이름만 입력받고 '\n' 소모 */

    /* 클라이언트 패킷 초기화 */
    int fname_len = strlen(clnt_pkt.buf);
    clnt_pkt.buf_len = fname_len;
    clnt_pkt.buf[fname_len] = '\0';
    clnt_pkt.cmd = FILE_REQ;

    /* 서버에 FILE REQUEST */
    if (write_all(sock, &clnt_pkt, sizeof(clnt_pkt)) == -1) {
        error_handling("write_all() error");
    }
    printf("[Tx] cmd: %d, file name: %s\n", clnt_pkt.cmd, clnt_pkt.buf);

    PACKET rcv_pkt;
    memset(&rcv_pkt, 0, sizeof(PACKET));
    int total_rx_cnt = 0;
    int total_rx_bytes = 0; /* buf_len을 더하라 */

    /* 응답 패킷 받고 출력 */
    while (1) {
        ssize_t read_size = read_all(sock, &rcv_pkt, sizeof(PACKET));
        if (read_size <= 0) error_handling("read_all() error");

        /* FILE_NOT_FOUND면 while loop escape */
        if (rcv_pkt.cmd == FILE_NOT_FOUND) {
            printf("[Rx] cmd: %d, %s: %s\n",
                    rcv_pkt.cmd, clnt_pkt.buf, rcv_pkt.buf);
            break;
        }
        total_rx_cnt += 1;
        total_rx_bytes += rcv_pkt.buf_len;

        fwrite(rcv_pkt.buf, 1, rcv_pkt.buf_len, stdout);
        fflush(stdout);

        /* 마지막 패킷 구조체를 받으면 */
        if(rcv_pkt.cmd == FILE_END){
            printf("\n---------------------------\n");
            printf("[Rx] cmd: %d, FILE_END\n", rcv_pkt.cmd);

            /* FILE_END_ACK 송신 */
            memset(&clnt_pkt, 0, sizeof(PACKET));
            clnt_pkt.cmd = FILE_END_ACK;
            if (write_all(sock, &clnt_pkt, sizeof(PACKET)) == -1) {
                error_handling("FILE_END_ACK write() error");
            }
            printf("[Tx] cmd: %d, FILE_END_ACK\n", clnt_pkt.cmd);
            break;
        }
        memset(&rcv_pkt, 0, sizeof(PACKET));
    }

    printf("------------------------------------\n");
    printf("Total Rx count: %d, bytes: %d\n", total_rx_cnt, total_rx_bytes);
    printf("TCP Client Socket Close!\n");
    printf("------------------------------------\n");
    /* ----------- VVboilerplate codeVV ----------- */

    close(sock);
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

ssize_t write_all(int sock, const void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(sock, (const char *)buf + total, len - total);
        if (n == -1) return -1;
        total += n;
    }
    return total;
}
