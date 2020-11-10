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

#define PORT 8080

int main(int argc, char const *argv[]) {
    int blocking = 0;
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char *hello = "Hello from server";

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if (blocking == 0) {
        fcntl(server_fd, F_SETFL, O_NONBLOCK);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *) &address,
             sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t * ) & addrlen);
        // fcntl(new_socket, F_SETFL, O_NONBLOCK);

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
                    valread = read(new_socket, buffer, 1024);
                    write(new_socket, buffer, 1024);
                    if (valread == 0) {
                        printf("Socket closed\n");
                        break;
                    } else {
                        printf("Message received and sent back\n");
                    }
                }
                close(new_socket);
                exit(0);
            } else close(new_socket);
        }
    }
    return 0;
}
