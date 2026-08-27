#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

void communicate_with_server(int sock) {
    char buffer[BUFFER_SIZE];
    ssize_t recv_size;

    while (1) {
        printf("Enter command: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove the newline character

        // If the command is "exit", terminate the communication
        if (strcmp(buffer, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        send(sock, buffer, strlen(buffer), 0);

        recv_size = recv(sock, buffer, BUFFER_SIZE, 0);
        if (recv_size > 0) {
            buffer[recv_size] = '\0';
            printf("Server response: %s\n", buffer);
        } else if (recv_size == 0) {
            printf("Server disconnected\n");
            break;
        } else {
            perror("recv");
            break;
        }
    }

    close(sock);
}

int main() {
    int sock;
    struct sockaddr_in server_addr;

    // Create the socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server %s:%d\n", SERVER_IP, SERVER_PORT);

    // Communicate with the server
    communicate_with_server(sock);

    return 0;
}
