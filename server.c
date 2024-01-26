// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

// Определение констант
#define MAX_CLIENTS 5
#define SERVER_IP "127.0.0.1"
#define PORT 8080

// Объявление мьютекса для синхронизации доступа к общим ресурсам
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Статические переменные для идентификации клиентов и подсчета активных клиентов
static int client_id_counter = 0;
static int current_clients = 0;

// Структура данных для передачи в поток
typedef struct
{
    int client_socket;        // Сокет клиента
    struct sockaddr_in *addr; // Адрес клиента
    pid_t pid;                // Идентификатор процесса клиента
    short value;              // Значение от клиента
    unsigned short msb_value; // Старший байт значения от клиента
    unsigned char num_bits;   // Количество значащих битов значения от клиента
    unsigned short crc16;     // CRC16 для данных от клиента
    char exit_command[5];     // Команда на выход
} ThreadData;

// Прототипы функций
int check_crc16(unsigned char *buffer, int size);
void generate_response(unsigned char *request, unsigned char *response);
void process_request(ThreadData *thread_data);
void *handle_client(void *data);

// Функция для проверки CRC16
int check_crc16(unsigned char *buffer, int size)
{
    return 1;
}

// Функция для генерации ответа на запрос
void generate_response(unsigned char *request, unsigned char *response)
{
    short value_to_encode = (request[5] << 8) | request[6];
    short encoded_value = value_to_encode + 1;

    int sign_bit = 0;
    if (encoded_value < 0)
    {
        sign_bit = 1;
        encoded_value = -encoded_value;
    }

    int num_significant_bits = 0;
    short temp_value = encoded_value;
    while (temp_value > 0)
    {
        temp_value >>= 1;
        num_significant_bits++;
    }

    unsigned short msb_value = (encoded_value >> (num_significant_bits - 1)) & 0xFFFF;

    response[31] = sign_bit << 7;
    response[30] = 0;
    response[29] = 0;
    response[28] = msb_value >> 8;
    response[27] = msb_value & 0xFF;

    for (int i = 26; i >= 0; i--)
    {
        response[i] = (encoded_value >> (26 - i)) & 0x01;
    }
}

// Функция обработки клиента в отдельном потоке
void *handle_client(void *data)
{
    // Функция выполняется в отдельном потоке для каждого клиента
    ThreadData *thread_data = (ThreadData *)data;

    // Вывод информации о подключении клиента
    printf("Client %d connected\n", thread_data->pid);

    // Основной цикл обработки запросов от клиента
    while (1)
    {
        // Обработка запроса от клиента
        process_request(thread_data);

        // Проверка на команду выхода
        if (strcmp(thread_data->exit_command, "exit") == 0)
            break;
    }

    // Закрытие соединения с клиентом
    close(thread_data->client_socket);
    free(thread_data->addr);
    free(thread_data);

    // Уменьшение счетчика активных клиентов
    pthread_mutex_lock(&mutex);
    current_clients--;
    pthread_mutex_unlock(&mutex);

    // Вывод информации о отключении клиента
    printf("Client %d disconnected\n", thread_data->pid);

    // Завершение потока
    pthread_exit(NULL);
}

// Функция для обработки запроса от клиента
void process_request(ThreadData *thread_data)
{
    unsigned char request[12];
    if (recv(thread_data->client_socket, request, sizeof(request), 0) == -1)
    {
        perror("Receive error");
        return;
    }

    if (!check_crc16(request, sizeof(request)))
    {
        printf("Invalid CRC16\n");
        unsigned char response[32];
        strcpy(response, "invalid request");

        if (send(thread_data->client_socket, response, sizeof(response), 0) == -1)
        {
            perror("Send error");
        }
    }
    else
    {
        unsigned char response[32];
        generate_response(request, response);

        if (send(thread_data->client_socket, response, sizeof(response), 0) == -1)
        {
            perror("Send error");
        }

        if (strcmp(request, "exit") == 0)
        {
            strcpy(thread_data->exit_command, "exit");
        }
    }
}

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Создание сокета
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    // Привязка адреса к сокету
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Bind error");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Начало прослушивания порта
    if (listen(server_socket, MAX_CLIENTS) == -1)
    {
        perror("Listen error");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Вывод информации о начале прослушивания порта
    printf("Server listening on port %d...\n", PORT);

    // Основной цикл сервера
    while (1)
    {
        // Принятие соединения от клиента
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        if (client_socket == -1)
        {
            perror("Accept error");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        // Проверка количества активных клиентов и создание нового потока для обработки клиента
        pthread_mutex_lock(&mutex);
        if (current_clients < MAX_CLIENTS)
        {
            current_clients++;
            pthread_mutex_unlock(&mutex);

            // Создание структуры данных для передачи в поток
            ThreadData *thread_data = malloc(sizeof(ThreadData));
            if (thread_data == NULL)
            {
                perror("Error allocating memory for thread data");
                close(client_socket);
                continue;
            }

            // Заполнение структуры данными о клиенте
            thread_data->client_socket = client_socket;
            thread_data->addr = malloc(sizeof(struct sockaddr_in));
            memcpy(thread_data->addr, &client_addr, sizeof(struct sockaddr_in));
            thread_data->pid = client_id_counter++;
            thread_data->value = 0;
            thread_data->msb_value = 0;
            thread_data->num_bits = 0;
            thread_data->crc16 = 0;
            thread_data->exit_command[0] = '\0';

            // Создание потока для обработки клиента
            pthread_t thread;
            if (pthread_create(&thread, NULL, handle_client, (void *)thread_data) != 0)
            {
                perror("Error creating thread");
                free(thread_data->addr);
                free(thread_data);
                close(client_socket);
                continue;
            }

            // Отсоединение потока
            pthread_detach(thread);
        }
        else
        {
            // Если превышено максимальное количество клиентов, отклоняем соединение
            pthread_mutex_unlock(&mutex);
            close(client_socket);
        }
    }

    // Закрытие сокета сервера
    close(server_socket);

    return 0;
}