/* 2022117156 박상준 */

#define BUF_SIZE 100
// cmd type
#define FILE_REQ 1
#define FILE_RES 2
#define FILE_END 3
#define FILE_END_ACK 4
#define FILE_NOT_FOUND 5

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
// #include <errno.h>

void error_handling(char *message);

typedef struct {
    int cmd;
    /*
     * 파일 이름의 길이 또는 실제 전송되는
     * 파일의 크기 저장
     */
    int buf_len;
    char buf[BUF_SIZE+1]; // null 자리
} PACKET;

int main(int argc, char *argv[]) {
    int serv_sock; /* 연결용 소켓 */
    int clnt_sock; /* 데이터 송수신용 소켓 */

    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;

    char message[] = "hello\n";

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 9, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; /*IPv4*/
    /* 현재 컴퓨터에 존재하는 모든
     * 네트워크 인터페이스(랜카드)의
     * IP 주소로 들어오는 연결을 수신 */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockadd*) &serv_addr,
                sizeof(serv_addr)) == -1 ) {
        error_handling("bind() error");
    }

    clnt_addr_size = sizeof(clnt_addr);
    clnt_sock = accept(
            serv_sock,
            (struct sockaddr*) &clnt_addr,
            &clnt_addr_size);

    if (clnt_sock == -1) {
        error_handling("accept() error");
    }

    /* -------- ^^boilerplate code^^ --------*/

    printf("------------------------------\n");
    printf("TCP Remote File View Server\n");
    printf("------------------------------\n");

    while (1) {
        /* 패킷 수신 */
        PACKET rcv_pkt = {0};
        PACKET send_pkt = {0};
        /* 안전빵 */
        memset(&rev_pkt, 0, sizeof(PACKET));
        memset(&send_pkt, 0, sizeof(PACKET));

        int rcv_pkt_size = read(clnt_sock, &rcv_pkt, sizeof(PACKET));
        if (rcv_pkt_size == -1) {
            error_handling("read() error");
        }

        if (rcv_pkt.cmd == FILE_REQ) {
            printf("[Rx] cmd: %d, file_name: %s\n", rcv_pkt.cmd,
                    rcv_pkt.buf);

            FILE *fp = fopen(rcv_pkt.buf, "r");

            /* 파일이 없으면 FILE_NOT_FOUND 송신 */
            if (fp == NULL) { /* file open error */
                send_pkt.cmd = FILE_NOT_FOUND;
                snprintf(send_pkt.buf, sizeof(send_pkt.buf),
                        "File Not Found\n");

                send_pkt.buf_len = strlen(send_pkt.buf);

                if (write(clnt_sock, &send_pkt, sizeof(PACKET)) == -1) {
                    error_handling("write() error");
                }

                printf("Tx cmd: %d, %s: File Not Found\n",
                        send_pkt.cmd, send_pkt.buf);
                break; /* 그냥 제어흐름용, while 문 지우고 고치기 */
            }
            /* 파일이 존재한다면 */
        }
    }

    /* -------- VVboilerplate code^^ --------*/

    close(clnt_sock);
    close(serv_sock);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
