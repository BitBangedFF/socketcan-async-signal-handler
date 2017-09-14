#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#define CAN_IF "can0"

static int socket_fd = -1;

static volatile sig_atomic_t exit_signal = 0;

static const char done_msg[] = "|";
static const char data_msg[] = "+";
static const char nodata_msg[] = "_";

static void terminate_handler(int sig)
{
    (void) sig;
    exit_signal = 1;
}

static void io_handler(int sig)
{
    (void) sig;
    struct can_frame rx_frame;

    if(socket_fd >= 0)
    {
        ssize_t read_status;

        do
        {
            read_status = read(socket_fd, &rx_frame, CAN_MTU);

            if(read_status == CAN_MTU)
            {
                (void) write(STDOUT_FILENO, data_msg, sizeof(data_msg));
            }
            else
            {
                (void) write(STDOUT_FILENO, nodata_msg, sizeof(nodata_msg));
            }
        }
        while(read_status > 0);
    }

    (void) write(STDOUT_FILENO, done_msg, sizeof(done_msg));
}

static void sigint_wait(void)
{
    printf("waiting (press CTRL-C to continue)\n");

    while(exit_signal == 0)
    {
        sleep(1);
    }

    exit_signal = 0;

    printf("\n\n");
}

static void register_sigint(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_flags = SA_RESTART;
    act.sa_handler = terminate_handler;

    printf("registering SIGINT signal\n");

    if(sigaction(SIGINT, &act, 0) < 0)
    {
        perror("sigaction(SIGINT)");
        exit(1);
    }

    printf("\n");
}

static void register_sigio(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_flags = SA_RESTART;
    act.sa_handler = io_handler;

    printf("registering SIGIO signal\n");

    if(sigaction(SIGIO, &act, 0) < 0)
    {
        perror("sigaction(SIGIO)");
        exit(1);
    }

    printf("\n");
}

static void block_sigio(
        sigset_t * const mask,
        sigset_t * const orig_mask)
{
    printf("blocking SIGIO\n");

    sigaddset(mask, SIGIO);

    if(sigprocmask(SIG_BLOCK, mask, orig_mask) < 0)
    {
        perror("sigprocmask(SIG_BLOCK)");
        exit(1);
    }

    printf("\n");
}

static void unblock_sigio(
        sigset_t * const mask)
{
    printf("unblocking SIGIO\n");

    if(sigprocmask(SIG_SETMASK, mask, NULL) < 0)
    {
        perror("sigprocmask(SIG_SETMASK)");
        exit(1);
    }
}

static void create_can_socket(void)
{
    struct ifreq ifr;
    struct sockaddr_can can_address;

    memset(&ifr, 0, sizeof(ifr));
    memset(&can_address, 0, sizeof(can_address));

    printf("creating CAN socket\n");

    socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    if(socket_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    strncpy(ifr.ifr_name, CAN_IF, IFNAMSIZ);

    if(ioctl(socket_fd, SIOCGIFINDEX, &ifr) < 0)
    {
        perror("ifindex on can");
        exit(1);
    }

    can_address.can_family = AF_CAN;
    can_address.can_ifindex = ifr.ifr_ifindex;

    if(bind(socket_fd, (struct sockaddr*) &can_address, sizeof(can_address)) < 0)
    {
        perror("bind");
        exit(1);
    }

    if(fcntl(socket_fd, F_SETOWN, getpid()) < 0)
    {
        perror("fcntl F_SETOWN");
        exit(1);
    }

    if(fcntl(socket_fd, F_SETFL, FASYNC | O_NONBLOCK) < 0)
    {
        perror("fcntl F_SETFL");
        exit(1);
    }

    printf("\n");
}

static void example(void)
{
    sigset_t mask;
    sigset_t orig_mask;

    sigemptyset(&mask);
    sigemptyset(&orig_mask);

    register_sigint();

    sigint_wait();

    create_can_socket();

    register_sigio();

    sigint_wait();

    block_sigio(&mask, &orig_mask);

    sigint_wait();

    unblock_sigio(&orig_mask);

    printf("\n");

    sigint_wait();

    if(socket_fd >= 0)
    {
        close(socket_fd);
        socket_fd = -1;
    }
}

int main (int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    const pid_t pid = getpid();

    printf("\n");
    printf("use CTRL-C (SIGINT) to continue when waiting\n");
    printf("\n");
    printf("process ID: %d\n", (int) pid);
    printf("to trace the signals run 'sudo strace -e trace=signal -p %d'\n", (int) pid);
    printf("\n");
    printf("data characters:\n");
    printf("'%c' - CAN data was read\n", data_msg[0]);
    printf("'%c' - CAN data was not available\n", nodata_msg[0]);
    printf("'%c' - end of signal handler\n", done_msg[0]);
    printf("\n");

    example();

    printf("done\n");

    (void) fflush(stdout);

    return 0;
}
