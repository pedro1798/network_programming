// 2022117156 박상준

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>

#define BUF_SIZE 256
#define MSG_SIZE 100

struct ip_mreq mreq; // 멀티캐스트 그룹 구조체 전역 변수로 선언
int recv_sock;

void error_handling(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// 자식 프로세스의 SIGTERM 시그널 핸들러
void sigterm_handler(int sig) {
    if (sig == SIGTERM) {
        // 가입된 멀티캐스트 그룹 탈퇴 (IP_DROP_MEMBERSHIP)
        setsockopt(recv_sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *)&mreq, sizeof(mreq));
        printf("Multicast drop membership and Exit\n");

        // 자식 프로세스에서 생성한 소켓 종료
        close(recv_sock);
        printf("[Child] recv_sock closed\n");

        exit(0);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <GroupIP> <PORT> <Name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* 프로그램 매개변수 로컬변수에 저장 */
    char *group_ip = argv[1];
    int port = atoi(argv[2]);
    char *name = argv[3];

    /* 멀티캐스트 주소 초기화 */
    if (inet_pton(AF_INET, group_ip, &mreq.imr_multiaddr.s_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    /* 입력받은 IP주소가 멀티캐스트 대역인지 검사 */
    if (!IN_MULTICAST(ntohl(mreq.imr_multiaddr.s_addr))) {
        fprintf(stderr, "Not a multicast address: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY); /* 기본 네트워크 인터페이스 사용 */

    pid_t pid = fork(); /* 프로세스 분기 */

    if (pid < 0) error_handling("fork() error!\n");
    else if (pid == 0) { /* 자식 프로세스는 pid = 0 assign */
        struct sigaction act;
        act.sa_handler = sigterm_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        if (sigaction(SIGTERM, &act, NULL) < 0) {
            error_handling("sigaction() error\n");
        }

        /* recv_sock 초기화 */
        recv_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (recv_sock < 0) error_handling("recv_sock() error\n");

        /* Multicast 데이터 수신용: recv_sock (SO_REUSEADDR 적용) */
        int reuse = 1;
        if (setsockopt(recv_sock, SOL_SOCKET, SO_REUSEADDR,
                    (void *)&reuse, sizeof(reuse)) < 0) {
            error_handling("setsockopt(SO_REUSEADDR) error\n");
        }

        struct sockaddr_in recv_addr;
        memset(&recv_addr, 0, sizeof(recv_addr));
        recv_addr.sin_family = AF_INET;
        recv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* 들어오는 주소 멀티캐스트 주소로? */
        recv_addr.sin_port = htons(port);

        if (bind(recv_sock,
                 (struct sockaddr *)&recv_addr,
                 sizeof(recv_addr)) < 0) {
            error_handling("bind() error\n");
        }

        /* recv_sock 소켓 멀티캐스트 주소 설정 */
        if (setsockopt(recv_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                       (void *)&mreq, sizeof(mreq)) < 0) {
                error_handling("setsockopt(IP_ADD_MEMBERSHIP) error\n");
        }

        char buf[BUF_SIZE];
        for (;;) {
            int str_len = recvfrom(recv_sock, buf, BUF_SIZE-1, 0, NULL, 0);
            if (str_len < 0) break;

            buf[str_len] = '\0';
            printf("%s", buf);
            fflush(stdout);
        }
        close(recv_sock);
        exit(EXIT_SUCCESS);
    } else { /* 부모 프로세스는 자식 pid를 assign */
        char input_msg[MSG_SIZE + 2];
        char send_buf[BUF_SIZE];
        time_t rawtime;
        struct tm *timeinfo;
        char time_str[16];

        /* Multicast 데이터 송신용: send_sock (IP_MULTICAST_TTL설정: 64) */
        int send_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (send_sock < 0) error_handling("send_sock() error");

        /* IP_MULTICAST_TTL 64*/
        int time_live = 64;
        if (setsockopt(send_sock,
                       IPPROTO_IP,
                       IP_MULTICAST_TTL,
                       (void *)&time_live,
                       sizeof(time_live)) < 0) {
            error_handling("setsockopt(IP_MULTICAST_TTL) error\n");
        }

        struct sockaddr_in mul_addr;
        memset(&mul_addr, 0, sizeof(mul_addr));
        mul_addr.sin_family = AF_INET;
        mul_addr.sin_addr = mreq.imr_multiaddr;
        mul_addr.sin_port = htons(port);

        while(fgets(input_msg, sizeof(input_msg), stdin) != NULL) {
            /* 입력에 줄바꿈이 없으면 */
            if (strchr(input_msg, '\n') == NULL) {
                int ch;
                /* 입력 버퍼에서 100글자 넘어가는 글자 비우기 */
                while ((ch = getchar()) != '\n' && ch != EOF);
                input_msg[MSG_SIZE] = '\n';
                input_msg[MSG_SIZE + 1] = '\0';
            }

            if (strcmp(input_msg, "q\n") == 0 ||
                strcmp(input_msg, "Q\n") == 0) {
                printf("SIGTERM: Multicast Receiver terminate!\n");
                kill(pid, SIGTERM); /* 자식 프로세스에게 SIGTERM 시그널 전송 */
                close(send_sock);
                printf("[Parent] send_sock closed\n");

                waitpid(pid, NULL, 0); /* 자식 프로세스 대기 */
                break;
            }

            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);

            snprintf(send_buf, sizeof(send_buf), "%s [%s] %s", time_str, name, input_msg);
            sendto(send_sock, send_buf, strlen(send_buf), 0,
                           (struct sockaddr *)&mul_addr, sizeof(mul_addr));
        }
    }
    return EXIT_SUCCESS;
}
