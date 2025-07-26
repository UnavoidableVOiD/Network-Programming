#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int sockfd;
char client_name[50] = "anonymous";

void receive_file(const char *sender, const char *filename, int filesize)
{
    mkdir("received_files", 0777);

    char base_name[256];
    snprintf(base_name, sizeof(base_name), "%s-%s-%s", sender, client_name, filename);

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "received_files/%s", base_name);

    int counter = 1;
    while (access(full_path, F_OK) == 0)
    {
        snprintf(full_path, sizeof(full_path), "received_files/%s-%d", base_name, counter++);
    }

    FILE *fp = fopen(full_path, "wb");
    if (!fp)
    {
        perror("Failed to open file for writing");
        return;
    }

    printf("Receiving file '%s' (%d bytes) from %s...\n", filename, filesize, sender);

    int bytes_received = 0;
    char buffer[BUFFER_SIZE];
    while (bytes_received < filesize)
    {
        int to_read = (filesize - bytes_received) > BUFFER_SIZE ? BUFFER_SIZE : (filesize - bytes_received);
        int n = read(sockfd, buffer, to_read);
        if (n <= 0)
        {
            printf("Connection lost during file transfer.\n");
            break;
        }
        fwrite(buffer, 1, n, fp);
        bytes_received += n;
    }

    fclose(fp);

    if (bytes_received == filesize)
        printf(" File saved as '%s'\n", full_path);
    else
        printf("⚠️ File '%s' received partially.\n", filename);
}

void *receive_handler(void *arg)
{
    char buffer[BUFFER_SIZE + 1];
    int n;

    while ((n = read(sockfd, buffer, BUFFER_SIZE)) > 0)
    {
        buffer[n] = '\0';

        if (strncmp(buffer, "/recvfile ", 10) == 0)
        {
            char sender[50], filename[256];
            int filesize;

            if (sscanf(buffer, "/recvfile %s %s %d", sender, filename, &filesize) == 3)
            {
                receive_file(sender, filename, filesize);
            }
            else
            {
                printf(" Malformed /recvfile header received.\n");
            }
        }
        else
        {
            printf("%s", buffer);
            fflush(stdout);
        }
    }

    printf("\n Disconnected from server.\n");
    exit(0);
}

void send_file(const char *target, const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        perror("File open error");
        return;
    }

    fseek(fp, 0, SEEK_END);
    int filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header), "/sendfile %s %s %d\n", target, filename, filesize);
    send(sockfd, header, strlen(header), 0);

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0)
    {
        send(sockfd, buffer, bytes_read, 0);
    }

    fclose(fp);
    printf("File '%s' sent successfully to %s.\n", filename, target);
}

int main()
{
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf(" Connected to SocketTalk server on port %d\n", PORT);
    printf(" Commands:\n");
    printf("   /name <your_name>            - Set your display name\n");
    printf("   /msg <target_name> <msg>     - Send private message\n");
    printf("   /sendfile <target> <file>    - Send a file\n");
    printf("   /exit                        - Exit chat\n\n");

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_handler, NULL);

    while (1)
    {
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strncmp(buffer, "/name ", 6) == 0)
        {
            sscanf(buffer + 6, "%s", client_name);
            send(sockfd, buffer, strlen(buffer), 0);
        }
        else if (strncmp(buffer, "/sendfile ", 10) == 0)
        {
            char target[50], filename[256];
            if (sscanf(buffer + 10, "%s %s", target, filename) == 2)
            {
                send_file(target, filename);
            }
            else
            {
                printf("Usage: /sendfile <target> <filename>\n");
            }
        }
        else if (strncmp(buffer, "/msg ", 5) == 0)
        {
            send(sockfd, buffer, strlen(buffer), 0);
        }
        else if (strcmp(buffer, "/exit") == 0)
        {
            printf(" Disconnecting...\n");
            close(sockfd);
            exit(0);
        }
        else
        {
            send(sockfd, buffer, strlen(buffer), 0);
        }
    }

    return 0;
}