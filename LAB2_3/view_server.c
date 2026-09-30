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
// #include <errno.h>

typedef struct {
    int cmd; // 4byte
    int buf_len; // 4byte
    char buf[BUF_SIZE+1]; // null 자리
} PACKET;

void error_handling(char *message);
int file_transmit(PACKET *rcv_pkt, int clnt_sock, int *pkt_cnt, int *total_tx_bytes);
ssize_t read_all(int sock, void *buf, size_t len);
ssize_t write_all(int sock, const void *buf, size_t len);

int main(int argc, char *argv[]) {
    int serv_sock; /* 연결용 소켓 */
    int clnt_sock; /* 데이터 송수신용 소켓 */

    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; /*IPv4*/
    /* 현재 컴퓨터에 존재하는 모든
     * 네트워크 인터페이스(랜카드)의
     * IP 주소로 들어오는 연결을 수신 */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *) &serv_addr,
                sizeof(serv_addr)) == -1 ) {
        error_handling("bind() error");
    }

    if (listen(serv_sock, 5) == -1) error_handling("listen() error");

    clnt_addr_size = sizeof(clnt_addr);
    clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_addr, &clnt_addr_size);

    if (clnt_sock == -1) {
        error_handling("accept() error");
    }

    /* -------- ^^boilerplate code^^ --------*/

    int pkt_cnt = 0;
    int total_tx_bytes = 0;

    printf("------------------------------\n");
    printf("TCP Remote File View Server\n");
    printf("------------------------------\n");

    /* 패킷 초기화 */
    PACKET rcv_pkt;
    memset(&rcv_pkt, 0, sizeof(PACKET));

    /* 클라이언트측에서 FILE_REQ 수신 no While !! 절차적 처리 */
    int rcv_pkt_size = read_all(clnt_sock, &rcv_pkt, sizeof(PACKET));
    if (rcv_pkt_size == -1) {
        error_handling("read() error");
    }
    rcv_pkt.buf[BUF_SIZE] = '\0';

    /* 처음 수신하는 패킷은 FILE_REQ cmd 만 허용 */
    if (rcv_pkt.cmd != FILE_REQ) error_handling("wrong cmd received");

    printf("[Rx] cmd: %d, file_name: %s\n", rcv_pkt.cmd, rcv_pkt.buf);

    /* 클라이언트에게 파일 전송하기 */
    if (!file_transmit(&rcv_pkt, clnt_sock, &pkt_cnt, &total_tx_bytes)) {
        /* 파일 정상적으로 전송 후 FILE_END_ACK 수신 처리 */
        memset(&rcv_pkt, 0, sizeof(PACKET)); /* 패킷 받을 구조체 초기화 */
        if (read_all(clnt_sock, &rcv_pkt, sizeof(PACKET)) == -1) {
            error_handling("read() error");
        }
        /* 파일 전송 후 수신받은 cmd가 FILE_END_ACK 아닐 시 예외처리 */
        if (rcv_pkt.cmd != FILE_END_ACK) error_handling("wrong cmd received");
    }

    printf("------------------------------\n");
    printf("Total Tx count: %d, bytes: %d\n", pkt_cnt, total_tx_bytes);
    printf("TCP Server Socket Close!\n");
    printf("------------------------------\n");

    /* -------- VVboilerplate code^^ --------*/

    /* 소켓 닫기 */
    close(clnt_sock);
    close(serv_sock);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int file_transmit(PACKET *rcv_pkt, int clnt_sock, int *pkt_cnt, int *total_tx_bytes) {
    PACKET send_pkt = {0};
    memset(&send_pkt, 0, sizeof(PACKET));

    FILE *fp = fopen(rcv_pkt->buf, "r");
    if (fp == NULL) { /* file open error */
        send_pkt.cmd = FILE_NOT_FOUND;
        snprintf(send_pkt.buf, sizeof(send_pkt.buf), "File Not Found");
        send_pkt.buf_len = strlen(send_pkt.buf);

        if (write_all(clnt_sock, &send_pkt, sizeof(PACKET)) == -1) {
            error_handling("write_all() error");
        }
        printf("[Tx] cmd: %d, %s: File Not Found\n",
                send_pkt.cmd, rcv_pkt->buf);
        return -1; /* 파일이 존재하지 않으면 -1 리턴하고 종료 */
    } else { /* 파일이 존재한다면 */
        /*
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        int chunk_num = file_size / BUF_SIZE;
        int expected_pkt_num = (file_size % BUF_SIZE == 0) ? chunk_num : (chunk_num + 1);
        rewind(fp);
        */

        *pkt_cnt = 0;
        *total_tx_bytes = 0;
        size_t read_bytes;

        while(1) {
            memset(&send_pkt, 0, sizeof(PACKET));

            /* 파일에서 100바이트를 읽고 패킷 버퍼에 저장 */
            read_bytes = fread(send_pkt.buf, 1, BUF_SIZE, fp);
            if (ferror(fp)) {
                fclose(fp);
                error_handling("fread() error");
            }

            send_pkt.cmd = FILE_RES;
            /* 마지막 패킷 cmd FILE_END 업데이트 */
            int c = fgetc(fp);
            if (c == EOF) send_pkt.cmd = FILE_END;   // 이번이 마지막 데이터
            else ungetc(c, fp);

            send_pkt.buf_len = (int)read_bytes;

            if (write_all(clnt_sock, &send_pkt, sizeof(PACKET)) == -1) {
                fclose(fp);
                error_handling("packet write() error");
            }
            *total_tx_bytes += read_bytes;

            printf("[Tx] cmd: %d, len: %d, total_tx_cnt: %d, total_tx_bytes: %d\n",
                    send_pkt.cmd, send_pkt.buf_len,
                    ++(*pkt_cnt), *total_tx_bytes);

            /* 파일을 다 읽었다면 while loop 탈출 */
            if (send_pkt.cmd == FILE_END) break;
            sleep(1); /* 1초 간격으로 보낸다 */
        }
    }

    fclose(fp); /* 파일 닫고 종료 */
    return 0;
}

ssize_t read_all(int sock, void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(sock, (char *)buf + total, len - total);
        if (n == 0) return 0;
        if (n == -1) return -1;
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
