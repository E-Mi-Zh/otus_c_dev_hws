#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>		/* for struct sockaddr_in */
#include <string.h>
#include <arpa/inet.h>		/* for inet_pton() */
#include <stdbool.h>
#include <net/ethernet.h>	/* for struct ether_header */
#include <glib.h>			/* for hadh table */
#include <unistd.h>			/* for write() */
#include <getopt.h>			/* for getopt() */
#include <ctype.h>			/* for isprint() */
#include <fcntl.h>			/* for open */
#include <signal.h>			/* for signals */
#include <sys/resource.h>	/* for getrlimit() */
#include <sys/stat.h>		/* for umask() */
#include <threads.h>		/* for threads */
#include <libcli.h>			/* for CLI */

#include "logger.h"

/* Один аргумент (адрес сервера) обязательный */
#define N_ARGS 1
#define MAC_STR_LEN 18
#define MAX_STR_LEN 255
#define PORT_DEFAULT 5555

/* Ethernet protocol ID's */
#define	ETHERTYPE_IP		0x0800	/* Internet Protocol version 4 (IPv4) */
#define	ETHERTYPE_ARP		0x0806	/* Address Resolution Protocol (ARP) */
#define	ETHERTYPE_IPV6		0x86dd	/* Internet Protocol version 6 (IPv6) */
/* TO-DO: add VLAN support */
//#define       ETHERTYPE_VLAN          0x8100          /* IEEE 802.1Q VLAN tagging */

const char *server_ip_str;
int server_port = PORT_DEFAULT;
bool need_fork = false;
GHashTable *mac_table;
unsigned long int pkt_count = 0;

/* Virtual Switch */
typedef struct switch_t {
	int sockfd;		/* client socket */
	struct sockaddr_in switch_addr;	/* switch address */
} switch_t;

/* MAC record in MAC table */
typedef struct mac_record_t {
	struct sockaddr_in *cli_addr;
	unsigned short age;
} mac_record_t;

volatile sig_atomic_t refresh_age_flag = false;

void parse_args(int argc, char *argv[]);
void print_usage(void);

void switch_init(switch_t * v_switch, const char *server_ip_str, int server_port);
/* return Ethernet frame type string */
char *eth_type_str(uint16_t ether_type);
void print_mac_table(void);

void daemonize(void);

/* SIGALRM handler */
void handle_alarm(int sig);
int setnonblocking(int sock);

void refresh_age(void);
int cli_task_func(void *data);

int main(int argc, char *argv[])
{
	switch_t v_switch = { 0 };
	char ether_data[ETHER_MAX_LEN];
	char eth_src[MAC_STR_LEN];
	char eth_dst[MAC_STR_LEN];
	struct ether_header *hdr = (struct ether_header *)ether_data;
	socklen_t switch_addr;
	int ether_datasz;
	gpointer value = NULL;
	mac_record_t *mac_record;
	thrd_t cli_task_id;
	struct sigaction act;
	GHashTableIter iter;
	gpointer key;

	loglevel_on(LERR);
	loglevel_on(LINFO);
	parse_args(argc, argv);

	if (need_fork) {
		printf("Daemonizing!\n");
		daemonize();
	}

	switch_init(&v_switch, server_ip_str, server_port);

	/* Will refresh MAC age every second */
	act.sa_handler = handle_alarm;
	act.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &act, NULL);
	alarm(1);

	/* create processes to send/receive data to/from virtual switch */
	if (thrd_create(&cli_task_id, cli_task_func, &mac_table) != thrd_success) {
		LOG_ERROR("Failed to create CLI thread: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (thrd_detach(cli_task_id) != thrd_success) {
		LOG_ERROR("Failed to detach CLI thread: %s\n", strerror(errno));
	}

	while (true) {
		/* read ethernet frame from virtual switch port */
		switch_addr = sizeof(v_switch.switch_addr);
		ether_datasz = recvfrom(v_switch.sockfd, ether_data, sizeof(ether_data), 0,
			     		(struct sockaddr *)&v_switch.switch_addr, &switch_addr);
		if (ether_datasz > 0) {
			snprintf(eth_dst, MAC_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
				 hdr->ether_dhost[0], hdr->ether_dhost[1],
				 hdr->ether_dhost[2], hdr->ether_dhost[3],
				 hdr->ether_dhost[4], hdr->ether_dhost[5]);
			eth_dst[MAC_STR_LEN - 1] = '\0';
			snprintf(eth_src, MAC_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
				 hdr->ether_shost[0], hdr->ether_shost[1],
				 hdr->ether_shost[2], hdr->ether_shost[3],
				 hdr->ether_shost[4], hdr->ether_shost[5]);
			eth_dst[MAC_STR_LEN - 1] = '\0';

			LOG_INFO("Received packet %ld from switch:\t", pkt_count);
			LOG_INFO("dst %s\tsrc %s\t", eth_dst, eth_src);
			LOG_INFO("type %s(%04x)\t", eth_type_str(ntohs(hdr->ether_type)), ntohs(hdr->ether_type));
			LOG_INFO("cli_port: %d\t", v_switch.switch_addr.sin_port);
			LOG_INFO("size %d\n", ether_datasz);

			/* MAC learning */
			if ((value = g_hash_table_lookup(mac_table, eth_src)) != NULL) {
				mac_record = (mac_record_t *) value;
				LOG_INFO("MAC %s found with addr %s and port %d\n", eth_src,
					inet_ntoa(mac_record->cli_addr->sin_addr), mac_record->cli_addr->sin_port);
				mac_record->age = 0;
				if (memcmp(mac_record->cli_addr, &v_switch.switch_addr, sizeof(struct sockaddr_in))) {
					/* update port number for given eth_src */
					memcpy(mac_record->cli_addr, &v_switch.switch_addr, sizeof(struct sockaddr_in));
					LOG_INFO("Changing old address %s, port %d to new address %s, %d\n",
					     inet_ntoa(v_switch.switch_addr.sin_addr), v_switch.switch_addr.sin_port,
					     inet_ntoa(mac_record->cli_addr->sin_addr), mac_record->cli_addr->sin_port);
				}
			} else {
				/* insert new record to table */
				mac_record = malloc(sizeof(mac_record_t));
				mac_record->cli_addr = malloc(sizeof(struct sockaddr_in));
				memcpy(mac_record->cli_addr, &v_switch.switch_addr, sizeof(struct sockaddr_in));
				mac_record->age = 0;
				LOG_INFO("MAC %s new, inserting with address %s, port %d\n", eth_src,
					inet_ntoa(mac_record->cli_addr->sin_addr), mac_record->cli_addr->sin_port);
				g_hash_table_insert(mac_table, g_strdup(eth_src), (gpointer) mac_record);
			}

			/* Frame forwarding */
			/* if dest in mac table, forward ethernet frame to it */
			if ((value = g_hash_table_lookup(mac_table, eth_dst)) != NULL) {
				mac_record = (mac_record_t *) value;
				if (mac_record->cli_addr->sin_port == v_switch.switch_addr.sin_port) {
					/* we don't need to forward this frame since dest is here */
					LOG_INFO("Destination is here, discarding!\n");
					continue;
				}
				sendto(v_switch.sockfd, ether_data, ether_datasz, 0, (struct sockaddr *)mac_record->cli_addr,
				       sizeof(*(mac_record->cli_addr)));
				LOG_INFO("Sent to MAC %s address %s, port %d\n", eth_dst,
					 inet_ntoa(mac_record->cli_addr->sin_addr), mac_record->cli_addr->sin_port);
			} else
			    if (!memcmp(eth_dst, "ff:ff:ff:ff:ff:ff", sizeof("ff:ff:ff:ff:ff:ff"))) {
				/* dest is broadcast address */
				LOG_INFO("Broadcast!\n");
				g_hash_table_iter_init(&iter, mac_table);
				while (g_hash_table_iter_next(&iter, &key, &value)) {
					mac_record = (mac_record_t *) value;
					if (!memcmp(key, eth_src, sizeof(eth_src))) {
						/* don't broadcast back to source addr */
						LOG_INFO("Skip broadcasting to src MAC %s address %s, port %d\n", eth_src,
						     inet_ntoa(mac_record->cli_addr->sin_addr), mac_record->cli_addr->sin_port);
						continue;
					}
					sendto(v_switch.sockfd, ether_data, ether_datasz, 0, (struct sockaddr *)mac_record->cli_addr,
					       sizeof(*(mac_record->cli_addr)));
					LOG_INFO("Broadcasted to MAC %s address %s, port %d\n", eth_dst,
					     inet_ntoa(mac_record->cli_addr->sin_addr), mac_record->cli_addr->sin_port);
				}
			} else {
				/* dest unknown, flood to every known port except source */
				LOG_INFO("Packet FLOOD! Broadcast!\n");
				g_hash_table_iter_init(&iter, mac_table);
				while (g_hash_table_iter_next(&iter, &key, &value)) {
					mac_record = (mac_record_t *) value;
					if (!memcmp(key, eth_src, sizeof(eth_src))) {
						/* don't broadcast back to source addr */
						LOG_INFO("Skip broadcasting to src MAC %s address %s, port %d\n", eth_src,
						     inet_ntoa(mac_record->cli_addr->sin_addr), mac_record->cli_addr->sin_port);
						continue;
					}
					sendto(v_switch.sockfd, ether_data, ether_datasz, 0, (struct sockaddr *)mac_record->cli_addr,
					       sizeof(*(mac_record->cli_addr)));
					LOG_INFO("Broadcasted to MAC %s address %s, port %d\n", eth_dst,
					     inet_ntoa(mac_record->cli_addr->sin_addr), mac_record->cli_addr->sin_port);
				}
			}
		}
		/* MAC aging */
		if (refresh_age_flag) {
			LOG_INFO("Time to refresh MAC's age!\n");
			refresh_age();
		}
	}

	g_hash_table_destroy(mac_table);

	exit(EXIT_SUCCESS);
}

void parse_args(int argc, char *argv[])
{
	int c;
	opterr = 0;

	while ((c = getopt(argc, argv, "dhv")) != -1) {
		switch (c) {
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
		case 'p':
			server_port = atoi(optarg);
			if ((server_port < 0) || (server_port > 65535)) {
				LOG_ERROR("Port number should be gt than 0 and lt than 65535!\n");
				exit(EXIT_FAILURE);
			}
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
	printf("v_switch - virtual switch.\n");
	printf("\n");
	printf("Creates primitive virtual L2 switch, which accepts Ethernet frames from socket and redirects them to other clients.\n");
	printf("\n");
	printf("Usage: v_switch [OPTIONS] ip_v4_server_address\n");
	printf("Options:\n");
	printf("-p port - specify port on server (default 5555);");
	printf("-d - daemonize;");
	printf("-v - verbose;");
	printf("-h - print this help and exit.");
	printf("\n");
	printf("Examples:\n");
	printf("./v_switch 192.168.1.10\n");
	printf("./v_switch -p 7000 192.168.1.10\n");
	printf("./v_switch -d -v 192.168.1.10\n");
}

void switch_init(switch_t *v_switch, const char *server_ip_str, int server_port)
{
	v_switch->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (v_switch->sockfd < 0) {
		LOG_DEBUG("fail to socket: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	setnonblocking(v_switch->sockfd);

	v_switch->switch_addr.sin_family = AF_INET;
	v_switch->switch_addr.sin_port = htons(server_port);
	if (inet_pton(AF_INET, server_ip_str, &v_switch->switch_addr.sin_addr) != 1) {
		LOG_DEBUG("fail to inet_pton: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if ((bind(v_switch->sockfd, (struct sockaddr *)&v_switch->switch_addr, sizeof(v_switch->switch_addr))) < 0) {
		LOG_DEBUG("Failed to bind socket: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	mac_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	LOG_INFO("Switch started at %s:%d\n", server_ip_str, server_port);
}

char *eth_type_str(uint16_t ether_type)
{
	switch (ether_type) {
	case ETHERTYPE_IP:
		return "IP";
		break;
	case ETHERTYPE_ARP:
		return "ARP";
		break;
	case ETHERTYPE_IPV6:
		return "IPv6";
		break;
	default:
		return "UNK";
		break;
	}
}

void print_table(gpointer key, gpointer value, gpointer userdata)
{
	int *entries_count = userdata;
	mac_record_t *mac_record = (mac_record_t *) value;

	(*entries_count)++;
	LOG_INFO("%d\t", *entries_count);
	LOG_INFO("MAC: %s\t", (char *)key);
	LOG_INFO("ADDR: %s\t", inet_ntoa(mac_record->cli_addr->sin_addr));
	LOG_INFO("PORT: %d\t", mac_record->cli_addr->sin_port);
	LOG_INFO("AGE: %d\n", mac_record->age);
}

void print_mac_table(void)
{
	int entries_count = 0;
	LOG_INFO("MAC table:\n");
	g_hash_table_foreach(mac_table, print_table, &entries_count);
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
		LOG_ERROR
		    ("ошибочные файловые дескрипторы %d %d %d",
		     fd0, fd1, fd2);
	}
}

void handle_alarm(int sig)
{
	(void)sig;
	refresh_age_flag = true;
}

gboolean process_mac_age(gpointer key, gpointer value, gpointer userdata)
{
	mac_record_t *mac_record = (mac_record_t *) value;
	int *max_age = userdata;

	if (mac_record->age > (*max_age)) {
		/* delete entry */
		LOG_INFO("Deleting old entry: ");
		LOG_INFO("MAC %s, addr %s, port %d\n", (char *)key, inet_ntoa(mac_record->cli_addr->sin_addr), mac_record->cli_addr->sin_port);
		return true;
	} else {
		mac_record->age++;
		return false;
	}
}

void refresh_age(void)
{
	int max_age = 20;
	print_mac_table();
	g_hash_table_foreach_remove(mac_table, process_mac_age, &max_age);
	refresh_age_flag = false;
	alarm(1);
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

int cmd_show_mac_table(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	GHashTableIter iter;
	gpointer key;
	gpointer value = NULL;
	(void)argc;
	(void)argv;
	(void)command;
	int entries_count = 0;
	mac_record_t *mac_record;

	cli_print(cli, "MAC table:\n");
	g_hash_table_iter_init(&iter, mac_table);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		entries_count++;
		mac_record = (mac_record_t *) value;
		cli_print(cli, "%d\t", entries_count);
		cli_print(cli, "MAC: %s\t", (char *)key);
		cli_print(cli, "ADDR: %s\t", inet_ntoa(mac_record->cli_addr->sin_addr));
		cli_print(cli, "PORT: %d\t", mac_record->cli_addr->sin_port);
		cli_print(cli, "AGE: %d\n", mac_record->age);
	}

	return CLI_OK;
}

int cli_task_func(void *data)
{
	struct sockaddr_in servaddr;
	struct cli_command *c;
	struct cli_def *cli;
	int on = 1, x, s;

	(void)data;

	cli = cli_init();
	cli_set_hostname(cli, "vswitch");
	cli_set_banner(cli, "Welcome to the Virtual Switch.");

	c = cli_register_command(cli, NULL, "show", NULL,
				 PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show switch variables");
	cli_register_command(cli, c, "mac_table", cmd_show_mac_table,
			     PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show MAC table entries");

	s = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(12345);
	bind(s, (struct sockaddr *)&servaddr, sizeof(servaddr));
	listen(s, 50);

	while ((x = accept(s, NULL, 0))) {
		cli_loop(cli, x);
		close(x);
	}

	cli_done(cli);

	return EXIT_SUCCESS;
}
