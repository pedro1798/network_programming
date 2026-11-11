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
#include <sys/socket.h>

void error_handling(char *);
ssize_t read_all(int sock, void *buf, size_t len);
ssize_t write_all(int sock, const void *buf, size_t len);

typedef struct {
    int cmd;
    char buf[BUF_SIZE+1];
} PACKET;

/*
 * 클라이언트가 close() 호출 후 종료되면, select() 검색 대상에서 해당
 * 클라이언트 삭제 및 모든 클라이언트가 종료되면 서버도 종료 (2점)
*/
int main(int argc, char *argv[]) {
    fd_set reads, cpy_reads;
    int fd_max, str_len, fd_num, i;
    struct timeval timeout;

    int serv_sock;
    int clnt_sock;

    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&clnt_addr, 0, sizeof(serv_addr));
    socklen_t clnt_addr_size;

    if (argc != 2) { /* 프로그램 매개변수 예외처리 */
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0); /* 서버 소켓 연결 */
    if (serv_sock == -1) { /* 서버 소켓 예외처리 */
        error_handling("socket() error");
    }

    /* 서버 주소 초기화 */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    /* bind(), 예외처리 */
    if (bind(serv_sock, (struct sockaddr *) &serv_addr,
                sizeof(serv_addr)) == -1) {
        error_handling("bind() error!\n");
    }

    /* listen(), 예외처리 */
    if (listen(serv_sock, 5) == -1) {
        error_handling("listen() error");
    }

    FD_ZERO(&reads); /* fd_set reads 초기화 */
    FD_SET(serv_sock, &reads); /* fd_set에 서버 소켓 등록(클라이언트 연결용) */

    fd_max = serv_sock; /* 아직 아무 클라이언트 연결 없으므로 serv_sock */

    int toggle = 0; /* 클라이언트가 한번 들어오면 1로 세팅, 1인데 (max_fd == 3) 이면 서버 종료 */

    while(1) {
        /* 모든 클라이언트가 종료했으면 */
        if (toggle && !FD_ISSET(4, &reads) && !FD_ISSET(5, &reads)) {
            break;
        }

        /* 매번 원본 fd_set reads를 깊은 복사해서 사용 */
        cpy_reads = reads;

        /* 타임아웃 초기화 */
        timeout.tv_sec = 5;
        timeout.tv_usec = 5000;

        /* 모든 while loop select 다시 선언 */
        if ((fd_num = select(fd_max+1, &cpy_reads, 0, 0, &timeout)) == -1)
            break;

        if (fd_num == 0) continue;

        for(i = serv_sock; i <= fd_max; i++) {
            /* 들어온 요청이 없으면 continue */
            if (!FD_ISSET(i, &cpy_reads)) continue;

            if (i == serv_sock) {
                /* 서버 소켓에 요청이 들어오면 새로운 클라이언트 연결 */
                clnt_addr_size = sizeof(clnt_addr);

                clnt_sock = accept(serv_sock,
                           (struct sockaddr *)&clnt_addr,
                           &clnt_addr_size);
                toggle = 1; /* 클라이언트가 들어왔던 기록을 남김 */

                if (clnt_sock == -1) error_handling("accecpt error().\n");

                FD_SET(clnt_sock, &reads); /* 새로운 클라이언트 reads에 등록 */

                if (clnt_sock > fd_max) fd_max = clnt_sock; // fd_max 갱신

            } else if (i == 6) { /* SENDER 클라이언트가 데이터를 전송하면 */
                /* 그대로 fd_set에 등록된 fd 4, 5 클라이언트에게 포워딩한다. */
                PACKET pkt;

                ssize_t n = read_all(i, &pkt, sizeof(pkt));
                /* SENDER가 먼저 연결을 종료해도 RECEIVER 연결종료를 기다림 */
                if (n == 0) {
                    FD_CLR(i, &reads);
                    close(i);
                    continue;
                }

                if (n == sizeof(pkt)) { /* SENDER에게 데이터를 정상적으로 수신했으면 */
                    for (int j = serv_sock + 1; j < fd_max; j++) {
                        /* reciver client 4, 5 */
                        if (FD_ISSET(j, &reads)) {
                            write_all(j, &pkt, sizeof(pkt));
                        }
                    }
                }

            } else {
                char buf[1024];

                ssize_t len = recv(i, buf, sizeof(buf), 0);

                if (len == 0) { /* 상대가 정상적으로 close() 함 (FIN 수신) */
                    FD_CLR(i, &reads);
                    close(i); /* 리시버 목록(4, 5)에서도 제거 */
                } else if (len < 0) { /* 비정상 종료 */
                    FD_CLR(fd, &reads);
                    close(i);
                } else continue;
            }
        }
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

ssize_t write_all(int sock, const void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(sock, (const char *)buf + total, len - total);
        if (n == -1) return -1;
        total += n;
    }
    return total;
}
