#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>

#define PORT 8080
#define BUFFER_SIZE 256

volatile sig_atomic_t sighup_received = 0;
int client_fd = -1;

void handle_sighup(int signo)
{
    (void)signo;
    sighup_received = 1;
}

int create_listener()
{
    int listen_fd;
    int opt = 1;
    struct sockaddr_in addr;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 3);

    printf("LISTENER: bound to port %d (fd %d)\n", PORT, listen_fd);
    return listen_fd;
}

void install_sighup_handler()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sighup;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP, &sa, NULL);

    printf("SIGNAL: SIGHUP handler installed.\n");
}

void mask_sighup(sigset_t *savedMask)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigprocmask(SIG_BLOCK, &mask, savedMask);

    printf("SIGNAL: SIGHUP blocked before pselect.\n");
}

void event_loop(int listen_fd, const sigset_t *origMask)
{
    int maxfd;

    while (1)
    {
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(listen_fd, &readset);

        if (client_fd != -1)
        {
            FD_SET(client_fd, &readset);
            maxfd = (client_fd > listen_fd) ? client_fd : listen_fd;
        }
        else
            maxfd = listen_fd;

        int ready = pselect(maxfd + 1, &readset, NULL, NULL, NULL, origMask);

        if (ready == -1)
        {
            if (errno == EINTR)
            {
                if (sighup_received)
                {
                    sighup_received = 0;
                    printf("EVENT: caught SIGHUP.\n");
                }
            }
            else
            {
                perror("pselect");
                break;
            }
        }

        if (ready > 0 && FD_ISSET(listen_fd, &readset))
        {
            struct sockaddr_in peer;
            socklen_t plen = sizeof(peer);
            int newfd = accept(listen_fd, (struct sockaddr *)&peer, &plen);

            printf("EVENT: incoming connection, fd %d\n", newfd);

            if (client_fd == -1)
            {
                client_fd = newfd;
                printf("SERVER: accepted client on fd %d\n", client_fd);
            }
            else
            {
                printf("SERVER: already have client, closing extra fd %d\n", newfd);
                close(newfd);
            }
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &readset))
        {
            char buf[BUFFER_SIZE];
            ssize_t n = read(client_fd, buf, BUFFER_SIZE - 1);

            if (n > 0)
            {
                buf[n] = '\0';
                printf("EVENT: read %zd bytes from client fd %d: %s\n", n, client_fd, buf);
            }
            else
            {
                printf("SERVER: client disconnected, closing fd %d\n", client_fd);
                close(client_fd);
                client_fd = -1;
            }
        }
    }
}

int main(void)
{
    sigset_t originalMask;
    int listen_fd = create_listener();

    install_sighup_handler();

    mask_sighup(&originalMask);

    event_loop(listen_fd, &originalMask);

    if (client_fd != -1)
        close(client_fd);
    close(listen_fd);

    sigprocmask(SIG_SETMASK, &originalMask, NULL);

    return 0;
}
