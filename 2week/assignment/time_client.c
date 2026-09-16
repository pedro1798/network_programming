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

void error_handling(char *message);

int recv_all(int sock, void *buf, size_t len);
int send_all(int sock, const void *buf, size_t len);

int main(int argc, char* argv[]) {
    int sock; 
    struct sockaddr_in serv_addr;
    int str_len = 0;
    int idx = 0, read_len = 0;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    /* sockaddr_in 구조체 변수에 IPv4, IP, port 할당*/

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr,
                sizeof(serv_addr)) == -1)
        error_handling("connect() error!");

    printf("Connected.........");

    /* #################### */

    while (1) {
        PACKET pkt = {0};

        printf("\nType a message(time or q): ");

        /* 사용자의 입력을 받음 */
        if (fgets(pkt.time_msg, sizeof(pkt.time_msg), stdin) != NULL) {
            size_t input_len = strlen(pkt.time_msg);
            /* 널 처리 */ 
            pkt.time_msg[input_len] = '\0';
        }
        /* 입력 검사 및 cmd 업데이트 */
        if (!strcmp(pkt.time_msg, "time") || 
            !strcmp(pkt.time_msg, "time\n")) {
            pkt.cmd = TIME_REQ;
        } else if (!strcmp(pkt.time_msg, "q") || 
            !strcmp(pkt.time_msg, "q\n")) {
            pkt.cmd = TIME_END;
        } else {
            printf("Wrong message.");
            continue;
        }
        /* 정상적으로 패킷이 보내졌으면 */
        if (send_all(sock, &pkt, sizeof(pkt)) == 0) {
            /* 보낸 패킷이 시간 요청이면 */
            if (pkt.cmd == TIME_REQ) {
                printf("[Client] Tx TIME_REQ\n");
                printf("[Client] Rx TIME_RES: ");

                PACKET recv_pkt = {0};

                if (recv_all(sock, &recv_pkt, sizeof(recv_pkt)) == 1) {
                    /* cmd 뛰어넘고 메시지만 출력 */
                    printf("%s", recv_pkt.time_msg+4);
                }
            } else if (pkt.cmd == TIME_END) {
                printf("[Client] Tx TIME_END\n");
                printf("Exit Client\n");
                break;
            }
        } else error_handling("recive() failed");
    }

    /* #################### */
    close(sock);

    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

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
