/* 2022117156 박상준 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#define BUF_SIZE 100
#define TIME_REQ 1
#define TIME_RES 2
#define TIME_END 3

typedef struct {
    int cmd;
    char time_msg[BUF_SIZE];
} PACKET;

void error_handling(char *time_msg);
void get_time(PACKET *time_msg);
int recv_all(int sock, void *buf, size_t len);
int send_all(int sock, const void *buf, size_t len);

int main(int argc, char* argv[]) {
    int serv_sock;
    int clnt_sock;

    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;

    socklen_t clnt_addr_size;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    if (serv_sock == -1) 
        error_handling("socket() error"); 

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; /* IPv4 */

    /* 
     * 모든 로컬 IPv4 네트워크 인터페이스에서 들어오는 연결 받음 
     * htonl(host to network long): host byte order → network byte order의 32비트 변환 * INADDR_ANY 의 값은 0이다.
     */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    if (listen(serv_sock, 5) == -1) 
        error_handling("listen() error");

    clnt_addr_size = sizeof(clnt_addr);

    /* 클라이언트 연결 요청 수락 */
    /* accept는 클라이언트의 소켓 디스크립터를 반환 */
    clnt_sock = accept(
            serv_sock, /* 서버의 연결 대기 소켓 디스크립터 */
            (struct sockaddr *) &clnt_addr, /* 연결 요청한 클라이언트 주소정보 */
            &clnt_addr_size);

    if (clnt_sock == -1) 
        error_handling("accecpt() error");

    printf("Connected client sock: %d\n", clnt_sock);

    /* 뜯어고쳐야됨 */
    /* #################### */

    while (1) {
        PACKET rcv_pkt = {0};
        PACKET res_pkt = {0};

        /* 수신 후 확인 */
        if (recv_all(clnt_sock, &rcv_pkt.cmd, sizeof(int)) == 1 &&
            recv_all(clnt_sock, &rcv_pkt.time_msg, sizeof(PACKET) - sizeof(int)) == 1)
            // rcv_pkt.cmd = ntohl(rcv_pkt.cmd);
        else { /* 오류나면 종료 */
            error_handling("receive failed");
            break;
        }

        /* 시간 요청이 들어오면 쿼리 검사 */
        /* 그냥 서버측에서도 한번 더 검사... */
        /* wrong message인 경유 cmd = 0 */
        printf("debug:: %d\n", rcv_pkt.cmd);
        if (rcv_pkt.cmd == TIME_REQ &&
                (!strcmp(rcv_pkt.time_msg, "time") ||
                 !strcmp(rcv_pkt.time_msg, "time\n"))) {
            printf("[Server] Rx TIME_REQ\n");
            res_pkt.cmd = TIME_RES;
            get_time(&res_pkt);

            /* 구조체 전체를 클라이언트에 write */
            if (!send_all(clnt_sock, &res_pkt, sizeof(PACKET))) { 
                printf("[Server] TIME_RES time: %s\n", res_pkt.time_msg);
            } else {
                error_handling("TIME_RES Tx failed\n");
            }
        }
        /* 종료 요청이 들어오면 while break */
        else if (rcv_pkt.cmd == TIME_END) {
            printf("[Server] Rx TIME_END\n");
            printf("Server closed\n");
            break;
        }
        /* 잘못된 입력 */
        else {
            fprintf(clnt_sock, "Wrong message.\n");
        }
    }

    close(clnt_sock);
    close(serv_sock);

    return 0;
}

void error_handling(char *time_msg) {
    fputs(time_msg, stderr);
    fputs('\n', stderr);
    exit(1);
}

void get_time(PACKET *PACKET) {
    time_t rawtime;
    struct tm *timeinfo;

    // 1. 현재 시간 가져오기
    time(&rawtime);

    // 2. 지역 시간(Local Time) 구조체로 변환하기
    timeinfo = localtime(&rawtime);

    strftime(PACKET->time_msg, sizeof(PACKET->time_msg), "time: %Y-%m-%d %H:%M:%S", timeinfo);
}

/* sizeof(PACKET) 단위로 데이터 수신 보장 함수 */
int recv_all(int sock, void *buf, size_t len) {
    unsigned char *p = buf; /* 버퍼 인덱스 포인터 */
    size_t received = 0; /* 받은 바이트 수 */

    while (received < len) { /* 받을 바이트 수 보다 덜 받았다면 */
        /* 버퍼에 이어서 남은 바이트만큼만 최대로 수신하라 */
        ssize_t n = recv(sock, p+received, len-received, 0);

        if (n == 0) return 0; /* 상대가 송신 종료해 남은 데이터 없으면 0 리턴 */

        if (n < 0) {
            /* Error INTeRrupted 면 그 지점부터 데이터를 다시 받음 */
            if (errno == EINTR) continue;
            /* 아니면 오류 반환 */
            return -1;
        }
        received += (size_t)n;
    }
    return 1; /* 정상적으로 수신 */
}

int send_all(int sock, const void *buf, size_t len) {
    /* const는 p가 가리키는 데이터를 수식, *p = 10 불가능. */
    const unsigned char *p = buf;

    while (len > 0) {
        ssize_t n = send(sock, p, len, 0);

        if (n < 0) {
            if (errno = EINTR) continue;
            return 1;
        }

        if (n == 0) return -1;

        /* p 자체는 업데이트 가능 */
        p += n;
        len -= (size_t)n;
    }
    return 0; /* 성공: 0 */
}
