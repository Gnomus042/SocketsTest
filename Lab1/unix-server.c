// Server side C/C++ program to demonstrate Socket programming
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/un.h>

# define SOCKET_FILE "./socket"

int main(int argc, char const *argv[]) {
    int blocking = 0;
    int server_fd, new_socket, valread;
    struct sockaddr_un address;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if (blocking == 0) {
        fcntl(server_fd, F_SETFL, O_NONBLOCK);
    }

    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_FILE);
    unlink(SOCKET_FILE);

    if (bind(server_fd, (struct sockaddr *) &address,
             strlen(address.sun_path) + sizeof (address.sun_family)) < 0) {
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    } else {
        printf("bind successful\n");
        fflush(stdout);
    }
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    } else {
        printf("listening\n");
        fflush(stdout);
    }
    socklen_t csize;
    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address,&csize);
        if (new_socket < 0) {
            if (blocking == 1) {
                perror("accept");
                exit(EXIT_FAILURE);
            } else {
                printf("waiting for connection\n");
                sleep(1);
            }
        } else {
            int pid = fork();
            if (pid < 0) {
                perror("ERROR on fork");
                exit(EXIT_FAILURE);
            }
            if (pid == 0) {
                // close(server_fd);
                while (1) {
                    char buffer[1024] = {0};
                    valread = read(new_socket, buffer, sizeof(buffer));
                    if (valread == 0) {
                        // printf("Socket closed\n");
                        break;
                    } else {
                        write(new_socket, buffer, 1024);
                        // printf("Message received and sent back\n");
                    }
                }
                close(new_socket);
                exit(0);
            } else close(new_socket);
        }
    }
    return 0;
}
