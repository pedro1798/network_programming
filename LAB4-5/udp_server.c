/* 2022117156 박상준 */

#define BOARD_SIZE 5
#define GAME_REQ 1
#define GAME_RES 2
#define GAME_END 3

#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

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
void draw_puzzle(char b1[BOARD_SIZE][BOARD_SIZE], char b2[BOARD_SIZE][BOARD_SIZE]);
int check_puzzle(char b1[BOARD_SIZE][BOARD_SIZE], char b2[BOARD_SIZE][BOARD_SIZE], char ch);

int main(int argc, char* argv[]) {
    int serv_sock;
    socklen_t clnt_addr_size;
    struct sockaddr_in serv_addr, clnt_addr;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    serv_sock = socket(PF_INET, SOCK_DGRAM, 0);

    if (serv_sock == -1)
        error_handling("UDP socket creation error");
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_addr,
                sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    /* =====================================================*/

    printf("---------------------------------\n");
    printf("  Finding Alphabet Game Server   \n");
    printf("---------------------------------\n");
    srand(time(NULL)); // 랜덤값을 현재 시간으로 초기화

    char b1[BOARD_SIZE][BOARD_SIZE] = {0};
    RES_PACKET res = {0};

    /* 서버 시작하고 랜덤 퀴즈 b1 생성 */
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            b1[i][j] = 'A' + (rand() % 26);
        }
    }

    draw_puzzle(b1, res.board);

    int str_len;

    REQ_PACKET req = {0};

    while(1) {
        clnt_addr_size = sizeof(clnt_addr);
        /* req 패킷을 수신한다 */
        str_len = recvfrom(serv_sock, &req, sizeof(REQ_PACKET), 0,
                (struct sockaddr *)&clnt_addr, &clnt_addr_size);

        if (req.cmd != GAME_REQ)
            error_handling("Wrong cmd.\n");

        /* 일단 수신받고 다찼는지 체크 */
        int is_full = 1;
        for (int i = 0; i < BOARD_SIZE; i++)
            for (int j = 0; j < BOARD_SIZE; j++)
                if (res.board[i][j] == 0) is_full = 0;

        printf("[Server] Rx cmd=%d, ch=%c\n", req.cmd, req.ch);

        res.result = check_puzzle(b1, res.board, req.ch);

        res.cmd = (is_full) ? GAME_END : GAME_RES;

        sendto(serv_sock, &res, sizeof(RES_PACKET), 0,
                (struct sockaddr*)&clnt_addr, clnt_addr_size);

        printf("[Server] Tx cmd=%d, result=%d\n", res.cmd, res.result);

        if (res.cmd == GAME_END) {
            printf("No empty space. Exit this program.\n");
            break;
        }

        draw_puzzle(b1, res.board);

        memset(&req, 0, sizeof(req));
    }
    /* =====================================================*/

    printf("Exit server program.\n");
    close(serv_sock);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void draw_puzzle(char b1[BOARD_SIZE][BOARD_SIZE], char b2[BOARD_SIZE][BOARD_SIZE]) {
    printf("+-------------------+ +-------------------+ \n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("| %c ", b1[i][j]);
        }
        printf("| ");
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("| %c ", (b2[i][j]) ? b2[i][j] : ' ' );
        }
        printf("|\n");
        printf("+-------------------+ +-------------------+ \n");
    }
    printf("\n");
}

int check_puzzle(char b1[BOARD_SIZE][BOARD_SIZE], char b2[BOARD_SIZE][BOARD_SIZE], char ch) {
    int cnt = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (b1[i][j] == ch && b2[i][j] == 0) {
                b2[i][j] = ch;
                cnt++;
            }
        }
    }
    return cnt;
}
