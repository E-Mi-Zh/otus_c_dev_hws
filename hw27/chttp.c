#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
//#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <asm-generic/socket.h>

#define MAX_EPOLL_EVENTS 128
#define BACKLOG 128
#define BUFSIZE 2048
#define STRLENMAX 255
#define N_ARGS 3

char dirname[STRLENMAX];

#define ADDRSIZE 16
char address[ADDRSIZE];
int port;

static struct epoll_event events[MAX_EPOLL_EVENTS];

typedef struct ep_data_t {
	int sockfd;
	char in_buf[BUFSIZE];
	char out_buf[BUFSIZE];
	size_t in_size;
	size_t out_size;
	char url[STRLENMAX];
	int url_len;
} ep_data_t;

/* Переводим сокет в неблокирующий режим */
int setnonblocking(int sock);

/* Читаем и парсим данные от клиента */
void do_read(ep_data_t* ep_data);

/* Отправляем ответ клиенту */
void do_write(ep_data_t* ep_data);

/* Обработчик ошибок */
void process_error(int fd);

/* Возвращает URL из текста запроса */
int parse_request(char* input_string, char* url);

void parse_args(int argc, char* argv[]);
void print_usage(void);

int main(int argc, char **argv)
{
//    char *p;
//    int port;

    int efd; /* epoll fd */
    int listenfd; /* socket to listen */
	struct sockaddr_in servaddr = { 0 };
    struct epoll_event listenev;
    int events_count;
    struct epoll_event connev;
    int nfds;
    int connfd;
	ep_data_t conn_data = {0};
	ep_data_t* p_conn_data = &conn_data;

	parse_args(argc, argv);

	chdir(dirname);
	signal(SIGPIPE, SIG_IGN);
	efd = epoll_create(MAX_EPOLL_EVENTS);
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse));

	setnonblocking(listenfd);
    /* заполняем структуру с параметрами сокета для прослушивания */
	servaddr.sin_family = AF_INET;
	//servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_addr.s_addr = inet_addr(address);
	servaddr.sin_port = htons(port);
	
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if (listen(listenfd, BACKLOG) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	
    /* заполняем структуру для epoll данных*/
	listenev.events = EPOLLIN | EPOLLET;
	listenev.data.fd = listenfd;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &listenev) < 0) {
		perror("epoll_ctl");
		exit(EXIT_FAILURE);
	}
		
    events_count = 1;
	for (;;) {
		nfds = epoll_wait(efd, events, MAX_EPOLL_EVENTS, -1);
		for (int i = 0; i < nfds; i++) {
			if (events[i].data.fd == listenfd) {
				connfd = accept(listenfd, NULL, NULL);
				if (connfd < 0) {
					perror("accept");
					continue;
				}
				if (events_count == MAX_EPOLL_EVENTS - 1) {
					close(connfd);
					continue;
				}
				setnonblocking(connfd);
				connev.data.ptr = &conn_data;
				conn_data.sockfd = connfd;
				connev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
				if (epoll_ctl
				    (efd, EPOLL_CTL_ADD, connfd, &connev) < 0) {
					perror("epoll_ctl");
					close(connfd);
					continue;
				}
				events_count++;
			} else {
				p_conn_data = (ep_data_t*) events[i].data.ptr;
				if (events[i].events & EPOLLIN) {
					//printf("Start read from socket %d\n", p_conn_data->sockfd);
					do_read(p_conn_data);
				}
				if (events[i].events & EPOLLOUT) {
					//printf("Write event!\n");
					do_write(p_conn_data);
				}
				if (events[i].events & EPOLLRDHUP)
					process_error(p_conn_data->sockfd);
				//printf("closing socket %d\n", p_conn_data->sockfd);
				/* Закрываем сокет только если мы в него уже что-то записали */
				/* Иначе возникали ситуации, когда приходило событие EPOLLOUT */
				/* до EPOLLIN, клиенту ничего не отдавалось, сокет закрывался */
				/* и клиент выдавал Connection reset by peer */
				if (p_conn_data->out_size != 0) {
					epoll_ctl(efd, EPOLL_CTL_DEL, p_conn_data->sockfd, &connev);
					close(p_conn_data->sockfd);
					memset(&conn_data, 0, sizeof(conn_data));
					events_count--;
				}
			}
		}
	}
	exit(EXIT_SUCCESS);
}

int setnonblocking(int sock)
{
	int opts;

	opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		perror("fcntl(F_GETFL)");
		return -1;
	}

	opts = (opts | O_NONBLOCK);
	if (fcntl(sock, F_SETFL, opts) < 0) {
		perror("fcntl(F_SETFL)");
		return -1;
	}
    
	return 0;
}

void do_read(ep_data_t* ep_data)
{
	char* found = NULL;
	int res = 0;

	while (found == NULL) {
		res = recv(ep_data->sockfd, ep_data->in_buf, BUFSIZE, 0);
		if (res < 0) {
			perror("read");
			return;
		}
		found = strstr(ep_data->in_buf, "\r\n\r\n");
	}
	ep_data->in_buf[res] = '\0';
	ep_data->in_size = res;
	//printf("read: %s\n", ep_data->in_buf);

	ep_data->url_len = parse_request(ep_data->in_buf, ep_data->url);
	//printf("URL: %s\n", ep_data->url);

}

void do_write(ep_data_t* ep_data)
{
	int res = 0;
	struct stat stat_buf;
	int filefd;

	if (ep_data->url_len != 0) {
		if (((res = access(ep_data->url, R_OK)) != 0) && (errno == EACCES)) {
			ep_data->out_size = snprintf(ep_data->out_buf, BUFSIZE-1, "HTTP/1.1 403 Forbidden\r\n\
Content-Length: %d\r\n\
Connection: close\r\n\
Content-Type: text/html\r\n\r\n\
<html><head><title>403 Forbidden</title></head>\
<body>\
<h1>Forbidden</h1>\
<p>You don't have permission to access /%s on this server!</p>\
</body></html>\n", (146 + ep_data->url_len), ep_data->url);
			ep_data->out_buf[ep_data->out_size] = '\0';
			res = send(ep_data->sockfd, ep_data->out_buf, ep_data->out_size, 0);
			if (res < 0) {
				perror("write");
				return;
			}
			return;
		}
		if ((filefd = open(ep_data->url, O_RDONLY)) == -1) {	/* open the file for reading */
			ep_data->out_size = snprintf(ep_data->out_buf, BUFSIZE-1, "HTTP/1.1 404 Not Found\r\n\
Content-Length: 133\r\n\
Connection: close\r\n\
Content-Type: text/html\r\n\r\n\
<html><head><title>404 Not Found</title></head><body><h1>Not Found</h1>The requested URL was not found on this server.</body></html>\n");
			ep_data->out_buf[ep_data->out_size] = '\0';
			res = send(ep_data->sockfd, ep_data->out_buf, ep_data->out_size, 0);
			if (res < 0) {
				perror("write");
				return;
			}
			//puts(ep_data->out_buf);
			return;
		}
		fstat(filefd, &stat_buf);
		ep_data->out_size = sprintf(ep_data->out_buf, "HTTP/1.1 200 OK\r\n\
Server: chttp\r\n\
Content-Length: %ld\r\n\
Connection: close\r\n\
Content-Type: text/html\r\n\r\n", stat_buf.st_size);
		ep_data->out_buf[ep_data->out_size] = '\0';
		res = send(ep_data->sockfd, ep_data->out_buf, ep_data->out_size, 0);
		if (res < 0) {
			perror("write");
			return;
		}
		//puts(ep_data->out_buf);
		res = sendfile(ep_data->sockfd, filefd, NULL, stat_buf.st_size);
		if (res < 0) {
			perror("write");
			close(filefd);
			return;
		}
		close(filefd);
		//shutdown(ep_data->sockfd, SHUT_RDWR);
	}
}

void process_error(int fd)
{
	printf("fd %d error!\n", fd);
}

int parse_request(char* input_string, char* url)
{
	char* token;

	token = strtok(input_string, " ");
	if (token == NULL) {
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
		url[0] = '\0';
        return 0;
    }

	token = strtok(NULL, " ");
	if (token == NULL) {
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
		url[0] = '\0';
        return 0;
    }
	/* Удаляем лидирующий слэш */
	token++;
    strncpy(url, token, STRLENMAX-1);
    url[STRLENMAX-1] = '\0';
	return strlen(url);
}

void parse_args(int argc, char* argv[]) {
    if (argc < N_ARGS) {
        fprintf(stderr, "Not all parameters are specified!\n");
        print_usage();
        exit(EXIT_FAILURE);
    }
    //char str[STRLENMAX];
	char* token;

    strncpy(dirname, argv[1], STRLENMAX-1);
    dirname[STRLENMAX-1] = '\0';
    token = strtok(argv[2], ":");
    strncpy(address, token, ADDRSIZE);
    dirname[ADDRSIZE] = '\0';
	//printf("token=%s\n", token);
    token = strtok(NULL, "");
	sscanf(token, "%d", &port);
	//printf("token=%s\n", token);
	//exit(EXIT_SUCCESS);
	// sscanf(argv[2], "%s:%d", &address);
    // if (threads_num < 1) {
    //     fprintf(stderr, "Incorrect threads number (should be greater or equal than 1): %d\n", threads_num);
    //     exit(EXIT_FAILURE);
    // }
}
    
void print_usage(void) {
    printf("chttp - Very simple HTTP server.\n");
    printf("Usage: chttp files_directory address:port\n");
    printf("Example: ./chttp file 127.0.0.1:5555\n");
}
