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

    PACKET clnt_pkt = {0};

    if (scanf("%s", &clnt_pkt.buf) == -1) {
        error_handling("scanf() error");
    }
    getchar();

    /* 클라이언트 패킷 초기화 */
    int fname_len = strlen(clnt_pkt.buf);
    clnt_pkt.buf_len = fname_len;
    clnt_pkt.buf[fname_len] = '\0';
    clnt_pkt.cmd = FILE_REQ;

    /* 서버에 FILE REQUEST */
    write(sock, &clnt_pkt, sizeof(clnt_pkt));

    /* ----------- VVboilerplate codeVV ----------- */

    close(sock);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
