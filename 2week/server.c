#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

void error_handling(char *message);

int main(int argc, char* argv[]) {
    /* 두 개의 소ㅔㅋㅅ 디스크립터 선언 */
    int serv_sock; // 서버 소켓
    int clnt_sock; // 클라이언트 소켓 

    struct sockaddr_in serv_addr; // IPv4용 구조체 
    struct sockaddr_in clnt_addr;

    socklen_t clnt_addr_size;

    char message[50];

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // IPv4, TCP
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) 
        error_handling("socket() error");

    // 메모리의 일부 구간을 특정 바이트 값으로 채움 
    // serv_addr의 모든 바이트를 0으로 채운다 
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // 서버가 쓰는 IP 주소 자동 할당 
    serv_addr.sin_addr.s_addr=htonl(INADDR.ANY);
    // 호스트 바이트 순서를	
    // 네트워크 바이트	순서(Big endian)로 변경
    serv_addr.sin_port=htons(atoi(argv[1]));

    // 서버 소켓을 바인드
    if (bind(serv_sock, (struct sockaddr*) &serv_addr,
                sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    clnt_addr_size = sizeof(clnt_addr);
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr,
            &clnd_addr_size);
    if (clnt_sock == -1) 
        error_handling("accept() error");

    time_t now = time(NULL);

    struct tm *t = localtime(&now);
    strftime(message, sizeof(message), 
            "%Y-%m-%d %H:%M:%S", t);

    write(clnt_sock, message, sizeof(message));
    close(clnt_sock);
    close(serv_sock);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
