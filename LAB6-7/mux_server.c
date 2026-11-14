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

// #pragma pack(push, 1)
typedef struct {
    int cmd;
    char buf[BUF_SIZE + 1];
} PACKET;
// #pragma pack(pop)

/* 요청한 바이트 전체 전송 */
ssize_t write_all(int sock, const void *buf, size_t len) {
    size_t total = 0;

    while (total < len) {
        ssize_t n = write(sock, (const char *)buf + total, len - total);

        if (n < 0) {
            if (errno == EINTR) /* 시그널에 의해 시스템 콜이 중단 */
                continue;
            return -1;
        }

        if (n == 0) {
            errno = EIO; /* 일반적인 입출력 오류 */
            return -1;
        }

        total += (size_t)n;
    }

    return (ssize_t)total;
}

/* 클라이언트를 select 감시 대상에서 제거하고 소켓 종료 */
void remove_client(int fd, fd_set *reads, int *active) {
    if (!FD_ISSET(fd, reads)) /* 이미 fd_set에 없으면 종료 */
        return;

    FD_CLR(fd, reads);
    (*active)--; /* 활성화된 fd -1 */

    if (close(fd) == -1)
        perror("close(client)");
}

void forward_receivers(const PACKET *pkt, fd_set *reads, int *active) {
    for (int fd = 4; fd <= 5; fd++) {
        if (!FD_ISSET(fd, reads))
            continue;

        /* fd 4, 5에 패킷 전달 */
        ssize_t n = write_all(fd, pkt, sizeof(*pkt));

        if (n < 0) {
            perror("write_all(receiver)");
            remove_client(fd, reads, active);
            continue;
        }

        // printf("[Forward] fd=%d, bytes=%zd, cmd=%d\n",
        printf("Forward client [6] ---> client [%d] (%zd bytes)\n", fd, n);
        fflush(stdout);
    }
}

int main(int argc, char *argv[]) {
    int serv_sock = -1;
    int fd_max = -1;
    int accepted = 0;
    int active = 0;
    int status = EXIT_SUCCESS;

    fd_set reads, cpy_reads;

    /* TCP 부분 수신 대응 */
    unsigned char sender_buffer[sizeof(PACKET)];
    size_t sender_used = 0;

    FD_ZERO(&reads);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *end = NULL;
    errno = 0;
    /* strtol: 문자열을 long type 정수로 */
    /* end:    숫자로 변환되지 않은 첫 번째 문자의 주소를 저장할 포인터 */
    long port = strtol(argv[1], &end, 10);

    if (errno != 0 || end == argv[1] ||
        *end != '\0' || port < 1 || port > 65535) { /* 포트 입력값 오류 */
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* 연결이 끊어진 Receiver에 서버가 write할 때 발생하는  SIGPIPE의 기본 동작인 강제 종료로
     * 서버가 멈추는 현상 방지 */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        /* SIGPIPE: 연결이 끊긴 곳에 데이터를 쓰려 함
         * SIG_IGN: 해당 시그널이 들어와도 무시하라
         * signal 함수가 실패하면 (== SIG_ERR) 예외처리
         */
        perror("signal(SIGPIPE)");
        /* 프로그램이 정상적으로 완료되지 않고 에러로 인해 비정상 종료되었음을
         * 운영체제(OS)에 알리는 exit status */
        return EXIT_FAILURE;
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int on = 1; /* 옵션을 활성화하겠다는 의미의 정수 변수 */
    if (setsockopt(serv_sock, SOL_SOCKET,
                   SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("setsockopt");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons((unsigned short)port);

    if (bind(serv_sock, (struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) < 0) {
        perror("bind");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    if (listen(serv_sock, 5) < 0) {
        perror("listen");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    FD_SET(serv_sock, &reads);
    fd_max = serv_sock;

    while (1) {
        /* 3개 클라이언트 모두 접속한 적이 있고
           현재 연결된 클라이언트가 없으면(모든 클라이언트가 종료하면) 종료 */
        if (accepted == 3 && active == 0) break;

        cpy_reads = reads;

        struct timeval timeout = {
            .tv_sec = 3,
            .tv_usec = 0
        };

        int fd_num = select(fd_max + 1, &cpy_reads, NULL, NULL, &timeout);

        if (fd_num < 0) {
            /* select()중 인터럽트가 들어와 중단되면 continue */
            if (errno == EINTR) continue;
            /* 실제 시스템 오류시 종료 */
            perror("select");
            status = EXIT_FAILURE;
            break;
        }

        /* 타임아웃은 continue */
        if (fd_num == 0) continue;

        for (int i = serv_sock; i <= fd_max; i++) {
            if (!FD_ISSET(i, &cpy_reads)) continue;

            /* 이번 반복에서 이미 제거된 소켓 방지 */
            if (!FD_ISSET(i, &reads)) continue;

            if (i == serv_sock) {
                /* serv_sock에 요청이 들어오면 해당 클라이언트 연결 */
                struct sockaddr_in clnt_addr;
                socklen_t addr_len = sizeof(clnt_addr);

                int clnt_sock = accept(
                    serv_sock,
                    (struct sockaddr *)&clnt_addr,
                    &addr_len
                );

                if (clnt_sock < 0) {
                    if (errno == EINTR || errno == ECONNABORTED) continue;
                    perror("accept");
                    status = EXIT_FAILURE;
                    goto cleanup;
                }

                /* 명세: Receiver 4, 5 → Sender 6
                 * FD_SETSIZE: select()가 사용하는 fd_set 구조체가 담을 수 있는
                 * 최대 파일 디스크립터 개수(리눅스 기준 보통 1024)를 정의한 매크로 상수*/

                if (clnt_sock != 4 + accepted || clnt_sock >= FD_SETSIZE) {
                    fprintf(stderr, "Unexpected client fd: %d\n", clnt_sock);
                    close(clnt_sock);
                    status = EXIT_FAILURE;
                    goto cleanup;
                }

                FD_SET(clnt_sock, &reads);

                if (clnt_sock > fd_max) fd_max = clnt_sock;

                accepted++;
                active++;

                // printf("[Accept] fd=%d\n", clnt_sock);
                printf("connected client: %d (fd max: %d)\n", clnt_sock, fd_max);
                fflush(stdout);

                /* 3 클라이언트 연결 완료 후  추가 연결 요청은 감시하지 않음 */
                if (accepted == 3) FD_CLR(serv_sock, &reads);

            } else if (i == 6) { /* Sender 수신 */
                ssize_t n = read(
                    i,
                    sender_buffer + sender_used,
                    sizeof(sender_buffer) - sender_used
                );

                int sender_done = 0;

                if (n > 0) {
                    sender_used += (size_t)n;

                    /* 패킷 하나가 완성된 경우만 전달 */
                    if (sender_used == sizeof(sender_buffer)) {
                        PACKET pkt;
                        memcpy(&pkt, sender_buffer, sizeof(pkt));
                        sender_used = 0;

                        if (pkt.cmd == MSG_SEND) { /* sender가 보낸 패킷 cmd가 적절하면 포워딩 */
                            forward_receivers( &pkt, &reads, &active);
                        } else { /* 아니면 예외처리 */
                            fprintf(stderr, "Unexpected sender cmd: %d\n", pkt.cmd);

                            sender_done = 1;
                        }
                    }
                } else if (n == 0) { /* Sender FIN 수신하면 */
                    if (sender_used != 0) {
                        fprintf(stderr, "Incomplete PACKET: %zu bytes\n", sender_used);
                    }
                    sender_done = 1;
                } else if (errno != EINTR) {
                    perror("read(sender)");
                    sender_done = 1;
                }

                if (sender_done) {
                    remove_client(6, &reads, &active);

                    /* Sender가 종료되면 Receiver들에게 TERMINATE 명령 전달 */
                    PACKET end_packet = {0};
                    end_packet.cmd = TERMINATE;

                    forward_receivers(&end_packet, &reads, &active);
                }

            } else if (i == 4 || i == 5) { /* Receiver 수신 */
                /* Receiver 종료 감지 */
                char buf[BUF_SIZE + 1];

                ssize_t n = recv(i, buf, sizeof(buf), 0);

                if (n == 0) { /* FIN 수신 */
                    remove_client(i, &reads, &active); /* 해당 클라이언트 삭제 */
                } else if (n < 0) {
                    if (errno == EINTR) continue; /* 인터럽트로 중지되면 그냥 continue */
                    perror("recv(receiver)");
                    remove_client(i, &reads, &active);
                }
                /* n > 0: Receiver가 보낸 예기치 않은 데이터를 읽고 폐기 */
            }
        }
    }

cleanup:
    /* 오류 종료 시에도 남아 있는 클라이언트 정리 */
    for (int fd = 4; fd <= 6; fd++) {
        if (FD_ISSET(fd, &reads)) remove_client(fd, &reads, &active);
    }

    /* 서버 리스닝 소켓은 마지막에 닫음 */
    if (serv_sock >= 0 && close(serv_sock) < 0) {
        perror("close(server)");
        status = EXIT_FAILURE;
    }

    return status;
}

