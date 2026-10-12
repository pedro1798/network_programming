/* 2022117156 박상준 */

#define BOARD_SIZE 5
#define GAME_REQ 1
#define GAME_RES 2
#define GAME_END 3

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    int cmd;
    char ch;
} REQ_PACKET;

typedef struct {
    int cmd;
    char board[BOARD_SIZE][BOARD_SIZE];
    int result;
} RES_PACKET;

void error_handling(char *message);
void draw_puzzle(char b1[BOARD_SIZE][BOARD_SIZE]);

int main(int argc, char* argv[]) {
    int sock;
    int str_len;
    socklen_t addr_size;
    struct sockaddr_in serv_addr, from_addr;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    /* ================================================ */

    printf("---------------------------------\n");
    printf("  Finding Alphabet Game Client\n");
    printf("---------------------------------\n");
    REQ_PACKET req = {0};
    RES_PACKET res = {0};
    srand(time(NULL));

    while (1) {
        // req 패킷 초기화
        req.cmd = GAME_REQ;
        req.ch = 'A' + (rand() % 26);

        // 서버에 전송
        sendto(sock, &req, sizeof(req), 0,
                (struct sockaddr *) &serv_addr,
                 sizeof(serv_addr));

        printf("[Client] Tx cmd=%d, ch=%c\n",
                req.cmd, req.ch);

        /* 전송하고 결과 수신받는다 */
        addr_size = sizeof(from_addr);
        str_len = recvfrom(sock, &res, sizeof(res), 0,
                   (struct sockaddr*)&from_addr, &addr_size);
        if (str_len < 0)
            error_handling("recvfrom() error");

        if (str_len != sizeof(req)) {
            fprintf(stderr, "Invalid request packet size.\n");
            continue;
        }

        if (res.cmd == GAME_REQ) error_handling("Wrong cmd\n");

        /* 수신받은 결과 터미널에 출력 */
        printf("[Client] Rx cmd=%d, result=%d\n",
                res.cmd, res.result);

        if (res.cmd == GAME_END) { /* 받은 cmd가 GAME_END면 클라이언트 종료 */
            printf("No empty space. Exit this program.\n");
            break;
        }

        draw_puzzle(res.board);

        memset(&req, 0, sizeof(req));
        memset(&res, 0, sizeof(res));
    }

    /* ================================================ */
    printf("Exit Client Program.\n");
    close(sock);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void draw_puzzle(char b1[BOARD_SIZE][BOARD_SIZE]) {
    printf("+-------------------+\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("| %c ", (b1[i][j]) ? b1[i][j] : ' ' );
        }
        printf("|\n");
        printf("+-------------------+\n");
    }
    printf("\n");
}
