#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <confuse.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/resource.h>

// #define LOCKFILE "demon.pid"
// #define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

/* имя сокета и файла для мониторинга хранятся в конфиг файле */
#define DEF_SOCKET "demon.sock"
char* socketname_opt = NULL;
char* socketname_conf = NULL;

#define DEF_FILE "file.txt"
char* filename_opt = NULL;
char* filename_conf = NULL;

#define DEF_CONF "demon.conf"
char* configname_opt = NULL;

bool need_fork = false;

bool finish_work = false;

#define FREE(x) do { \
                    if (x != NULL) { \
                        free(x); \
                        x = NULL; \
                    }} while(0)

#define LOG_PRINT(level, fmt, ...) do { \
    if (need_fork) { \
        syslog(level, fmt "%s", __VA_ARGS__); \
    } else { \
        fprintf(stderr, "demon.c: "); \
        fprintf(stderr, fmt "%s", __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } } while (0)


#define LOGERROR(...) LOG_PRINT(LOG_ERR, __VA_ARGS__, "")
#define LOGINFO(...) LOG_PRINT(LOG_INFO, __VA_ARGS__, "")

/* Читает конфиг файл с использованием libconfuse*/
void read_config(const char* config_name);

/* Разбирает опции командной строки и устанавливает флаги (перекрывают заданные в конфиге). */
void parse_options(int argc, char* argv[]);
void print_usage(void);
off_t get_file_size(const char* filename);

void sigterm_handler(int signo);
void sighup_handler(int signo);
const char* configname(void);
const char* socketname(void);
const char* filename(void);
void daemonize(char* cmd);

// int lockfile(int fd);
// int already_running(void);

int main(int argc, char* argv[])
{
    int sock, msgsock, rval;
    struct sockaddr_un server;
    char buf[1024];
    int res = 0;

    // if (already_running()) {
    //     LOGERROR("Daemon is already running!");
    //     exit(EXIT_FAILURE);
    // }
    parse_options(argc, argv);

    if (need_fork) {
        LOGINFO("daemonizing!");
        daemonize("demon_prog");
    }
sleep(1);

    if (signal(SIGINT, sigterm_handler) == SIG_ERR)
        LOGERROR("error processing signal SIGINT");
    if (signal(SIGTERM, sigterm_handler) == SIG_ERR)
        LOGERROR("error processing signal SIGTERM");
    if (signal(SIGHUP, sighup_handler) == SIG_ERR)
        LOGERROR("error processing signal SIGHUP");

    read_config(configname());

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        LOGERROR("Fail to open socket %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    server.sun_family = AF_UNIX;

    strcpy(server.sun_path, socketname());
    LOGINFO("socket has number %d and name %s", sock, server.sun_path);    
    res = bind(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_un));
    if (res < 0)
    {
        LOGERROR("binding stream socket error: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    LOGINFO("socket binded code %d", res);    
    
    res = listen(sock, 5);
    if (res < 0) {
        LOGERROR("socket listen error: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while(!finish_work) {
        msgsock = accept(sock, 0, 0);
        if (msgsock == -1) {
            if (!finish_work) {
                LOGERROR("socket accept failure: %s", strerror(errno));
            }
            break;
        } else {
            LOGINFO("accepting connection.");
            // do {
                memset(buf, 0, sizeof(buf));
                if ((rval = read(msgsock, buf, 1024)) < 0) {
                    if (!finish_work) 
                        LOGERROR("reading stream message failure: %s", strerror(errno));
                } else if (rval == 0) {
                    LOGINFO("ending connection");
                } else {
                    sprintf(buf, "File %s size = %ld bytes", filename(), get_file_size(filename()));
                    LOGINFO("%s", buf);
                    send(msgsock, buf, strlen(buf), 0);
                }
            // } while (rval > 0);
        }
        close(msgsock);
    }

    close(sock);
    unlink(socketname());

    FREE(socketname_conf);
    FREE(filename_conf);
    exit(EXIT_SUCCESS);
}

void parse_options(int argc, char* argv[])
{
    int res;
    while ((res = getopt(argc, argv, "c:s:f:dh")) != -1) {
        switch (res) {
            case 'c':
                configname_opt = optarg;
                break;
            case 's':
                socketname_opt = optarg;
                break;
            case 'f':
                filename_opt = optarg;
                break;
            case 'd':
                need_fork = true;
                break;
            default:
                print_usage();
                exit(EXIT_FAILURE);
        }
    }
}

void read_config(const char* config_name)
{
    cfg_opt_t opts[] = {
        CFG_SIMPLE_STR("socket", &socketname_conf),
        CFG_SIMPLE_STR("file", &filename_conf),
        CFG_END()
        };
    cfg_t *cfg;

    cfg = cfg_init(opts, 0);
	cfg_parse(cfg, config_name);
    cfg_free(cfg);
}

void print_usage(void)
{   
    printf("demon - daemon which prints some file size by request (through socket).\n");
    printf("Default behaviour: listen to socket defined in demon.conf, monitor file defined in monitor.conf\n");
    printf("Usage: demon [options] \n");
    printf("Options:\n");
    printf("\t-c config\tset config filename (default demon.conf)\n");
    printf("\t-s socket\tset socket name (default get from config)\n");
    printf("\t-f file\t\tset file name (default get from config)\n");    
    printf("\t-d\t\tdaemonize\n");
    printf("\t-h\t\tprint usage\n");
}

off_t get_file_size(const char* filename)
{
    struct stat file_status;
    if (stat(filename, &file_status) < 0) {
        LOGERROR("can't get size of file %s: %s", filename, strerror(errno));
        return -1;
    }

    return file_status.st_size;
}
void sigterm_handler(int signo)
{
    if (signo == SIGINT) {
        LOGINFO("got termination signal (SIGINT), exiting.");
    } else if (signo == SIGTERM) {
        LOGINFO("got termination signal (SIGTERM), exiting.");
    }
    finish_work = true;
}

void sighup_handler(int signo)
{
    if (signo == SIGHUP) {
        LOGINFO("got reload signal (SIGHUP), rereading config.");
        read_config(configname());
    }
}

const char* socketname(void)
{
    if (socketname_opt != NULL) {
        return socketname_opt;
    } else if (socketname_conf != NULL) {
        return socketname_conf;
    } else {
        return DEF_SOCKET;
    }
}

const char* filename(void)
{
    if (filename_opt != NULL) {
        return filename_opt;
    } else if (filename_conf != NULL) {
        return filename_conf;
    } else {
        return DEF_FILE;
    }
}

const char* configname(void)
{
    if (configname_opt != NULL) {
        return configname_opt;
    } else {
        return DEF_CONF;
    }
}

void daemonize(char* cmd)
{
    struct sigaction sa;
    struct rlimit rl;
    pid_t pid;
    int fd0, fd1, fd2;

    /* Инициализировать файл журнала. */
    openlog(cmd, LOG_CONS, LOG_DAEMON);
    
    /* Сбросить маску режима создания файла. */
    umask(0);
    
    /* Получить максимально возможный номер дескриптора файла. */
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        LOGERROR("can't get max file descriptor number: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Стать лидером нового сеанса, чтобы утратить управляющий терминал. */
    if ((pid = fork()) < 0) {
        LOGERROR("can't fork: %s. Exiting!", strerror(errno));
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
    if (sigaction(SIGHUP, &sa, NULL) < 0) LOGERROR("невозможно игнорировать сигнал SIGHUP");

    if ((pid = fork()) < 0) {
        LOGERROR("can't fork second time: %s. Exiting!", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        if (pid != 0) /* родительский процесс */ 
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
    closelog();
    for (unsigned long int i = 0; i < rl.rlim_max; i++) {
        close(i);
    }

    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    /* reopen syslog */
    openlog(cmd, LOG_CONS, LOG_DAEMON);

    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        LOGERROR("ошибочные файловые дескрипторы %d %d %d", fd0, fd1, fd2);
    }
}

#if 0
int lockfile(int fd)
{
    struct flock fl;
 
    fl.l_type = F_WRLCK;
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;
 
    return (fcntl(fd, F_SETLK, &fl));
}

int already_running(void)
{
    int fd;
    char buf[16];
    fd = open(LOCKFILE, O_RDWR | O_CREAT, LOCKMODE);
    if (fd < 0) {
        LOGERROR("невозможно открыть %s: %s", LOCKFILE, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (lockfile(fd) < 0) {
        if (errno == EACCES || errno == EAGAIN) {
            close(fd);
            return 1;
        }
        LOGERROR("невозможно установить блокировку на %s: %s", LOCKFILE, strerror(errno));
        exit(EXIT_FAILURE);
    }

    ftruncate(fd, 0);
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf) + 1);
    
    return 0;
}
#endif