// Client side C/C++ program to demonstrate Socket programming
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#define PORT 8080
#define PACKET_SIZE 1024

char *read_file(char *file_name) {
    FILE *fp = fopen(file_name, "r");

    if (fp == NULL) {
        perror("Error while opening the file.\n");
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0L, SEEK_END);
    int sz = ftell(fp);
    rewind(fp);

    char ch;
    char *res;
    res = malloc(sizeof(char) * (sz/PACKET_SIZE+sz%PACKET_SIZE)*PACKET_SIZE);
    int idx = 0;
    while ((ch = fgetc(fp)) != EOF) {
        res[idx] = ch;
        idx++;
    }
    fclose(fp);
    return res;
}

int main(int argc, char const *argv[]) {
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char *text = read_file("text.txt");
    char buffer[PACKET_SIZE] = {0};
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    clock_t start = clock();
    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    clock_t stop = clock();
    printf("Time to connect: %.20lf\n", (stop-start)/(double)CLOCKS_PER_SEC);


    double time = 0;
    for (int i = 0; i < strlen(text) / PACKET_SIZE; i++) {
        start = clock();
        send(sock, text+(i*PACKET_SIZE), PACKET_SIZE, 0);
        // printf("Message sent:\n%.10s\n\n", text+(i*PACKET_SIZE));
        valread = read(sock, buffer, PACKET_SIZE);
        // printf("Current buffer:\n%.10s\n\n", buffer);
        stop = clock();
        time += stop - start;
    }
    start = clock();
    close(sock);
    stop = clock();
    printf("Time to close: %.20lf\n", (stop-start)/(double)CLOCKS_PER_SEC);
    time = time / CLOCKS_PER_SEC;
    printf("Packets per second: %f\n", (double)strlen(text) / PACKET_SIZE / time);
    printf("Bytes per second: %f\n", (double)strlen(text) / time);
    return 0;
}
