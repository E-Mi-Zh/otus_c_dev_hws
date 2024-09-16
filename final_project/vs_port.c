#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>		/* for struct sockaddr_in */
#include <string.h>
#include <arpa/inet.h>		/* for inet_pton() */
#include <linux/if.h>		/* for struct ifreq */
#include <fcntl.h>		/* for open */
#include <linux/if_tun.h>	/* for TUN/TAP flags */
#include <sys/ioctl.h>		/* for ioctl() */
#include <unistd.h>		/* for close() */
#include <threads.h>		/* for threads */
#include <stdbool.h>
#include <net/ethernet.h>	/* for struct ether_header */
#include <getopt.h>		/* for getopt() */
#include <ctype.h>		/* for isprint() */
#include <signal.h>		/* for signals */
#include <sys/resource.h>	/* for getrlimit() */
#include <sys/stat.h>		/* for umask() */

#include "logger.h"

/* Один аргумент (адрес сервера) обязательный */
#define N_ARGS 1

/* Максимальная длина имени сетевого интерфейса */
/* определена в ядре (include/uapi/linux/if.h): */
/* #define IFNAMSIZ 16 */
#define IFNAME_DEFAULT "vs_port0"
#define PORT_DEFAULT 5555

/* Virtual Switch Port */
typedef struct vs_port_t {
	char name[IFNAMSIZ];	/* interface name */
	int tapfd;		/* TAP device file descriptor, connected to Linux kernel network stack */
	int sockfd;		/* client socket, for communicating with virtual switch */
	struct sockaddr_in switch_addr;	/* virtual switch address */
} vs_port_t;

const char *server_ip_str;
const char *ifname_str = NULL;
int server_port = PORT_DEFAULT;
bool need_fork = false;

void parse_args(int argc, char *argv[]);
void print_usage(void);

/* init VSPort instance: create TAP device and client socket */
void vs_port_init(vs_port_t * vs_port, const char *server_ip_str, int server_port);
/* creates TAP device */
int tap_alloc(char *device_name);

/* Send packets to virtual switch */
int send_task_func(void *raw_vs_port);
/* Receive packets from virtual switch */
int receive_task_func(void *raw_vs_port);
/* Bringing up tap device interface */
int if_up(int sockfd, char *device_name);

void daemonize(void);

int main(int argc, char *argv[])
{
	vs_port_t vs_port = { 0 };
	thrd_t send_task_id;
	thrd_t receive_task_id;

	loglevel_on(LERR);
	loglevel_on(LINFO);
	parse_args(argc, argv);

	if (need_fork) {
		printf("Daemonizing!\n");
		daemonize();
	}

	LOG_DEBUG("Options parsed!\n");
	vs_port_init(&vs_port, server_ip_str, server_port);

	/* create processes to send/receive data to/from virtual switch */
	if (thrd_create(&send_task_id, send_task_func, &vs_port) != 0) {
		LOG_ERROR("Failed to create sending thread: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (thrd_create(&receive_task_id, receive_task_func, &vs_port) != 0) {
		LOG_ERROR("Failed to create receiving thread: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* bringing interface up */
	if_up(vs_port.sockfd, vs_port.name);

	if (thrd_join(send_task_id, NULL) != 0 || thrd_join(receive_task_id, NULL) != 0) {
		LOG_ERROR("fail to pthread_join: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

void parse_args(int argc, char *argv[])
{
	int c;
	opterr = 0;

	while ((c = getopt(argc, argv, "i:dp:hv")) != -1) {
		switch (c) {
		case 'i':
			ifname_str = optarg;
			break;
		case 'p':
			server_port = atoi(optarg);
			if ((server_port < 0) || (server_port > 65535)) {
				LOG_ERROR("Port number should be gt than 0 and lt than 65535!\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'd':
			need_fork = true;
			break;
		case 'v':
			loglevel_on(LDEBUG);
			break;
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
			break;
		case '?':
			if ((optopt == 'i') || (optopt == 'p'))
				LOG_ERROR("Option -%c requires an argument.\n", optopt);
			else if (isprint(optopt))
				LOG_ERROR("Unknown option `-%c'.\n", optopt);
			else
				LOG_ERROR("Unknown option character `\\x%x'.\n", optopt);
			print_usage();
			exit(EXIT_FAILURE);
		default:
			exit(EXIT_FAILURE);
		}
	}
	if ((argc - optind) < N_ARGS) {
		LOG_ERROR("Not all parameters are specified!\n");
		print_usage();
		exit(EXIT_FAILURE);
	}

	server_ip_str = argv[optind];
}

void print_usage(void)
{
	printf("vs_port - virtual switch port.\n");
	printf("\n");
	printf
	    ("Creates TAP device (default name vs_port0) and redirects all traffic through it (in and out)\n");
	printf("to virtual switch on specified IPv4 address (default port 5555).\n");
	printf("Requires root privileges or CAP_NET_ADMIN capability.");
	printf("\n");
	printf("Usage: vs_port [OPTIONS] ip_v4_server_address\n");
	printf("Options:\n");
	printf("-i interface_name - specify interface name (default is vs_port0);");
	printf("-p port - specify port on server (default 5555);");
	printf("-d - daemonize;");
	printf("-v - verbose;");
	printf("-h - print this help and exit.");
	printf("\n");
	printf("Examples:\n");
	printf("./vs_port 192.168.1.10\n");
	printf("./vs_port -p 7000 192.168.1.10\n");
	printf("./vs_port -i tap0 -d 192.168.1.10\n");
}

void vs_port_init(struct vs_port_t *vs_port, const char *server_ip_str, int server_port)
{
	if (ifname_str == NULL) {
		strncpy(vs_port->name, IFNAME_DEFAULT, IFNAMSIZ);
	} else {
		strncpy(vs_port->name, ifname_str, IFNAMSIZ);
	}

	vs_port->tapfd = tap_alloc(vs_port->name);
	if (vs_port->tapfd < 0) {
		LOG_ERROR("Failed to allocate tap device: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	vs_port->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (vs_port->sockfd < 0) {
		LOG_ERROR("Failed to create socket: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	vs_port->switch_addr.sin_family = AF_INET;
	vs_port->switch_addr.sin_port = htons(server_port);
	if (inet_pton(AF_INET, server_ip_str, &vs_port->switch_addr.sin_addr) != 1) {
		LOG_ERROR("fail to inet_pton: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	LOG_INFO("TAP device name: %s, Switch address: %s:%d\n", vs_port->name, server_ip_str, server_port);
}

int tap_alloc(char *device_name)
{
	(void)device_name;
	struct ifreq ifr = { 0 };
	int fd;
	int res;

	if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
		return fd;
	}

	/* IFF_TAP   - TAP device */
	/* IFF_NO_PI - Do not provide packet information */
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	/* TODO: try IFF_NAPI / IFF_NAPI_FRAGS */
	/* TODO: try IFF_MULTI_QUEUE */
	strncpy(ifr.ifr_name, device_name, IFNAMSIZ);

	if ((res = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
		LOG_DEBUG("Ioctl error %d: %s", res, strerror(errno));
		close(fd);
		return res;
	}

	return fd;
}

/* Send ethernet frame from TAP device to VSwitch */
int send_task_func(void *raw_vs_port)
{
	vs_port_t *vs_port = (vs_port_t *) raw_vs_port;
	char ether_data[ETHER_MAX_LEN];
	struct ether_header *hdr = (struct ether_header *)ether_data;
	int ether_datasz;
	ssize_t sendsz;

	while (true) {
		// read ethernet from tap device
		ether_datasz = read(vs_port->tapfd, ether_data, sizeof(ether_data));
		if (ether_datasz > 0) {
			// forward ethernet frame to switch
			sendsz = sendto(vs_port->sockfd, ether_data, ether_datasz, 0,
				   (struct sockaddr *)&vs_port->switch_addr, sizeof(vs_port->switch_addr));
			if (sendsz != ether_datasz) {
				LOG_DEBUG("sendto size mismatch: ether_datasz=%d, sendsz=%ld\n", ether_datasz, sendsz);
			}

			LOG_INFO("Sent to switch:\t");
			LOG_INFO("dst %02x:%02x:%02x:%02x:%02x:%02x\t",
				 hdr->ether_dhost[0], hdr->ether_dhost[1],
				 hdr->ether_dhost[2], hdr->ether_dhost[3],
				 hdr->ether_dhost[4], hdr->ether_dhost[5]);
			LOG_INFO("src %02x:%02x:%02x:%02x:%02x:%02x\t",
				 hdr->ether_shost[0], hdr->ether_shost[1],
				 hdr->ether_shost[2], hdr->ether_shost[3],
				 hdr->ether_shost[4], hdr->ether_shost[5]);
			LOG_INFO("type %04x\t", ntohs(hdr->ether_type));
			LOG_INFO("size %d\n", ether_datasz);
		}
	}
	return 0;
}

/* Receive ethernet frame from switch to TAP device */
int receive_task_func(void *raw_vs_port)
{
	vs_port_t *vs_port = (vs_port_t *) raw_vs_port;
	char ether_data[ETHER_MAX_LEN];
	struct ether_header *hdr = (struct ether_header *)ether_data;
	socklen_t switch_addr;
	int ether_datasz;
	ssize_t sendsz;

	LOG_INFO("Receiving on port %d\n", vs_port->sockfd);
	while (true) {
		// read ethernet frame from VSwitch
		switch_addr = sizeof(vs_port->switch_addr);
		ether_datasz = recvfrom(vs_port->sockfd, ether_data, sizeof(ether_data), 0,
			     (struct sockaddr *)&vs_port->switch_addr, &switch_addr);
		if (ether_datasz > 0) {
			sendsz = write(vs_port->tapfd, ether_data, ether_datasz);
			if (sendsz != ether_datasz) {
				LOG_DEBUG("sendto size mismatch: ether_datasz=%d, sendsz=%ld\n", ether_datasz, sendsz);
			}

			LOG_INFO("Received from switch:\t");
			LOG_INFO("dst %02x:%02x:%02x:%02x:%02x:%02x\t",
				 hdr->ether_dhost[0], hdr->ether_dhost[1],
				 hdr->ether_dhost[2], hdr->ether_dhost[3],
				 hdr->ether_dhost[4], hdr->ether_dhost[5]);
			LOG_INFO("src %02x:%02x:%02x:%02x:%02x:%02x\t",
				 hdr->ether_shost[0], hdr->ether_shost[1],
				 hdr->ether_shost[2], hdr->ether_shost[3],
				 hdr->ether_shost[4], hdr->ether_shost[5]);
			LOG_INFO("type %04x\t", ntohs(hdr->ether_type));
			LOG_INFO("size %d\n", ether_datasz);
		}
	}
	return 0;
}

int if_up(int sockfd, char *device_name)
{
	struct ifreq ifr = { 0 };
	int res;

	strncpy(ifr.ifr_name, device_name, IFNAMSIZ);
	ifr.ifr_flags |= IFF_UP;
	if ((res = ioctl(sockfd, SIOCSIFFLAGS, &ifr)) < 0) {
		LOG_ERROR("Can't bring interface up, ioctl error %d: %s", res, strerror(errno));
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}

void daemonize(void)
{
	struct sigaction sa;
	struct rlimit rl;
	pid_t pid;
	int fd0, fd1, fd2;

	/* Инициализировать файл журнала. */
	//openlog(cmd, LOG_CONS, LOG_DAEMON);
	loginit("vswitch.log");

	/* Сбросить маску режима создания файла. */
	umask(0);

	/* Получить максимально возможный номер дескриптора файла. */
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
		LOG_ERROR("can't get max file descriptor number: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Стать лидером нового сеанса, чтобы утратить управляющий терминал. */
	if ((pid = fork()) < 0) {
		LOG_ERROR("can't fork: %s. Exiting!", strerror(errno));
		exit(EXIT_FAILURE);
	} else {
		if (pid != 0) {
			/* родительский процесс */
			exit(EXIT_SUCCESS);
		}
	}

	setsid();

	/* Обеспечить невозможность обретения управляющего терминала в будущем.  */
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	// if (signal(SIGINT, sigterm_handler) == SIG_ERR)
	//     LOGERROR("demon.c: error processing signal SIGINT");
	// if (signal(SIGTERM, sigterm_handler) == SIG_ERR)
	//     LOGERROR("demon.c: error processing signal SIGTERM");
	// if (signal(SIGHUP, sighup_handler) == SIG_ERR)
	//     LOGERROR("demon.c: error processing signal SIGHUP");
	if (sigaction(SIGHUP, &sa, NULL) < 0) {
		LOG_ERROR("невозможно игнорировать сигнал SIGHUP");
	}

	if ((pid = fork()) < 0) {
		LOG_ERROR("can't fork second time: %s. Exiting!", strerror(errno));
		exit(EXIT_FAILURE);
	} else {
		if (pid != 0)	/* родительский процесс */
			exit(EXIT_SUCCESS);
	}

	/* Назначить корневой каталог текущим рабочим каталогом, */
	/* чтобы впоследствии можно было отмонтировать файловую систему. */
	// if (chdir("/tmp") < 0) {
	//     syslog(LOG_CRIT, "невозможно сделать текущим рабочим каталогом /");
	//     exit(EXIT_FAILURE);
	// }

	/* Закрыть все открытые файловые дескрипторы. */
	if (rl.rlim_max == RLIM_INFINITY) {
		rl.rlim_max = 1024;
	}

	/* closing syslog explicitly */
	//closelog();
	for (unsigned long int i = 0; i < rl.rlim_max; i++) {
		close(i);
	}

	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);

	/* reopen syslog */
	//openlog(cmd, LOG_CONS, LOG_DAEMON);
	loginit("vswitch.log");

	if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
		LOG_ERROR("ошибочные файловые дескрипторы %d %d %d", fd0, fd1, fd2);
	}
}