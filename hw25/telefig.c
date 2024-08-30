#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define BUF_SIZE 5000
#define N_ARGS 3
#define SERVER "telehack.com"
#define PROTO "telnet"

void print_usage(void) {
    printf("telefig - print figlet using telehack.com.\n");
    printf("Usage: telefig /fontname text\n");
    printf("Example: ./telefig /isometric1 Linux\n");
}

void parse_args(int argc) {
    if (argc < N_ARGS) {
        fprintf(stderr, "Not all parameters are specified!\n");
        print_usage();
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char *argv[]) {
    struct addrinfo hints = {0};
    struct addrinfo *result;
    struct addrinfo *rp;
    int res;
    int sockfd;
    char *command;
    size_t len;
    ssize_t nread;
    char buf[BUF_SIZE];

    parse_args(argc);

    /* Obtain address(es) matching host/port. */
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;
    hints.ai_protocol = IPPROTO_TCP;

    res = getaddrinfo(SERVER, PROTO, &hints, &result);
    if (res != 0) {
        fprintf(stderr, "Error resolving hostname %s: %s\n", argv[1], gai_strerror(res));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully connect(2).
        If socket(2) (or connect(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            continue;
        }
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(sockfd);
    }
    freeaddrinfo(result);           /* No longer needed */

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect to server %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    /* Skipping negotiation messages and server banner */
    while ((nread = recv(sockfd, &buf, BUF_SIZE, 0)) > 0) {
        //printf("got %ld\n", nread);
        continue;
    }

    len = snprintf(NULL, 0, "figlet /%s %s\r\n", argv[1], argv[2]);
    command = malloc(len + 1);
    snprintf(command, len + 1, "figlet /%s %s\r\n", argv[1], argv[2]);
    //printf("Sending %ld bytes\t %s\n", len, command);

    if ((nread = send(sockfd, command, len, 0)) != (ssize_t) (len)) {
        fprintf(stderr, "Sending error: wanted %ld, transmitted only %ld - %s\n", len, nread, strerror(errno));
        free(command);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    len = 0;
    memset(buf, 0, BUF_SIZE);
    while ((nread = recv(sockfd, &buf[len], BUF_SIZE - len, 0)) > 0) {
        len = len + nread;
        //printf("got %ld\n", nread);
    }

    for (size_t i = strlen(command); i < len-1; i++) {
        printf("%c", buf[i]);
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    exit(EXIT_SUCCESS);
}