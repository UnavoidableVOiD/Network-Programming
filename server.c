#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

typedef struct
{
    int socket;
    char name[50];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
int server_running = 1;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast(const char *message, int exclude_sock)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket != exclude_sock)
        {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

Client *get_client_by_name(const char *name)
{
    for (int i = 0; i < client_count; i++)
    {
        if (strcmp(clients[i].name, name) == 0)
        {
            return &clients[i];
        }
    }
    return NULL;
}

void remove_client(int sock)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket == sock)
        {
            close(clients[i].socket);
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg)
{
    int client_sock = *((int *)arg);
    free(arg);

    char name[50] = "anonymous";
    char buffer[BUFFER_SIZE];

    while (1)
    {
        int n = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0)
            break;

        buffer[n] = '\0';

        if (strncmp(buffer, "/name ", 6) == 0)
        {
            char new_name[50];
            sscanf(buffer + 6, "%s", new_name);

            pthread_mutex_lock(&clients_mutex);
            int name_exists = 0;
            for (int i = 0; i < client_count; i++)
            {
                if (strcmp(clients[i].name, new_name) == 0)
                {
                    name_exists = 1;
                    break;
                }
            }

            if (name_exists)
            {
                pthread_mutex_unlock(&clients_mutex);
                char msg[] = " Name already taken. Choose another.\n";
                send(client_sock, msg, strlen(msg), 0);
            }
            else
            {
                for (int i = 0; i < client_count; i++)
                {
                    if (clients[i].socket == client_sock)
                    {
                        strcpy(clients[i].name, new_name);
                        strcpy(name, new_name);
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                char msg[100];
                snprintf(msg, sizeof(msg), " Name set to %s\n", new_name);
                send(client_sock, msg, strlen(msg), 0);
            }
        }
        else if (strncmp(buffer, "/msg ", 5) == 0)
        {
            char target[50], *msg_body;
            sscanf(buffer + 5, "%s", target);
            msg_body = strchr(buffer + 5, ' ');
            if (msg_body)
            {
                msg_body++;
                Client *target_client = get_client_by_name(target);
                if (target_client)
                {
                    char msg[BUFFER_SIZE];
                    snprintf(msg, sizeof(msg), "[Private from %s]: %s\n", name, msg_body);
                    send(target_client->socket, msg, strlen(msg), 0);
                }
                else
                {
                    char msg[] = "User not found.\n";
                    send(client_sock, msg, strlen(msg), 0);
                }
            }
        }
        else if (strncmp(buffer, "/sendfile ", 10) == 0)
        {
            char target[50], filename[256];
            int filesize;

            if (sscanf(buffer + 10, "%s %s %d", target, filename, &filesize) == 3)
            {
                Client *target_client = get_client_by_name(target);
                if (target_client)
                {
                    char header[BUFFER_SIZE];
                    snprintf(header, sizeof(header), "/recvfile %s %s %d\n", name, filename, filesize);
                    send(target_client->socket, header, strlen(header), 0);

                    int bytes_forwarded = 0;
                    char file_buffer[BUFFER_SIZE];
                    while (bytes_forwarded < filesize)
                    {
                        int to_read = (filesize - bytes_forwarded) > BUFFER_SIZE ? BUFFER_SIZE : (filesize - bytes_forwarded);
                        int r = recv(client_sock, file_buffer, to_read, 0);
                        if (r <= 0)
                            break;
                        send(target_client->socket, file_buffer, r, 0);
                        bytes_forwarded += r;
                    }
                }
                else
                {
                    char msg[] = "File target user not found.\n";
                    send(client_sock, msg, strlen(msg), 0);
                }
            }
            else
            {
                char msg[] = "Invalid /sendfile command.\n";
                send(client_sock, msg, strlen(msg), 0);
            }
        }
        else
        {
            char msg[BUFFER_SIZE + 50];
            snprintf(msg, sizeof(msg), "[%s]: %s\n", name, buffer);
            broadcast(msg, client_sock);
        }
    }

    printf("Client disconnected: %s\n", name);
    remove_client(client_sock);
    return NULL;
}

void *server_input_handler(void *arg)
{
    char input[BUFFER_SIZE];
    while (fgets(input, sizeof(input), stdin) != NULL && server_running)
    {
        input[strcspn(input, "\n")] = 0;

        if (strncmp(input, "/kick ", 6) == 0)
        {
            char target[50];
            sscanf(input + 6, "%s", target);
            Client *target_client = get_client_by_name(target);
            if (target_client)
            {
                send(target_client->socket, "You have been kicked by the server.\n", 40, 0);
                printf("%s was kicked.\n", target);
                remove_client(target_client->socket);
            }
            else
            {
                printf("User not found.\n");
            }
        }
        else if (strcmp(input, "/shutdown") == 0)
        {
            printf("Shutting down server...\n");
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < client_count; i++)
            {
                send(clients[i].socket, "Server is shutting down.\n", 29, 0);
                close(clients[i].socket);
            }
            client_count = 0;
            pthread_mutex_unlock(&clients_mutex);
            server_running = 0;
            exit(0);
        }
        else
        {
            char msg[BUFFER_SIZE + 20];
            snprintf(msg, sizeof(msg), "[Server]: %s\n", input);
            broadcast(msg, -1);
        }
    }
    return NULL;
}

int main()
{
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0)
    {
        perror("Socket failed");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_sock, 10) < 0)
    {
        perror("Listen failed");
        exit(1);
    }

    printf("SocketTalk server running on port %d...\n", PORT);
    printf("Server Commands:\n");
    printf("  /kick <name>       - Kick a client\n");
    printf("  /shutdown          - Shut down server\n");
    printf("  <message>          - Broadcast message to all\n\n");

    pthread_t admin_thread;
    pthread_create(&admin_thread, NULL, server_input_handler, NULL);

    while (server_running)
    {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &len);
        if (*client_sock < 0)
        {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        clients[client_count].socket = *client_sock;
        strcpy(clients[client_count].name, "anonymous");
        client_count++;
        pthread_mutex_unlock(&clients_mutex);

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_sock);

        printf("New client connected.\n");
    }

    close(server_sock);
    return 0;
}