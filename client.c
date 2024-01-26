// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// IP-адрес и порт сервера
#define SERVER_IP "127.0.0.1"
#define PORT 8080

// Функция для вычисления CRC16
unsigned short calculate_crc16(unsigned char *data, int size)
{
    // В данном примере всегда возвращаем 0x1234 как CRC16
    return 0x1234;
}

// Функция для получения ID процесса
pid_t get_process_id()
{
    return getpid();
}

// Функция для ввода с консоли 16-битного знакового целого числа (тип short) (Байтов 5 и 6)
short get_signed_short_from_console()
{
    short value;
    printf("Enter a signed short value: ");
    scanf("%hd", &value);
    return value;
}

// Функция для генерации запроса к серверу
void generate_request(unsigned char *request)
{
    // Получение уникального ID процесса
    pid_t pid = get_process_id();
    request[0] = (pid >> 8) & 0xFF; // Байт 0 (старший байт ID клиента)
    request[1] = pid & 0xFF;        // Байт 1 (младший байт ID клиента)

    // Заглушка: адрес слова в восьмеричном виде 012
    request[2] = 0; // Байт 2
    request[3] = 1; // Байт 3
    request[4] = 2; // Байт 4

    // Заглушка: целое число со знаком (тип short)
    short value = get_signed_short_from_console();
    request[5] = (value >> 8) & 0xFF; // Байт 5 (старший байт)
    request[6] = value & 0xFF;        // Байт 6 (младший байт)

    // Заглушка: цена старшего разряда - 500
    unsigned short msb_value = 500;
    request[7] = (msb_value >> 8) & 0xFF; // Байт 7 (старший байт)
    request[8] = msb_value & 0xFF;        // Байт 8 (младший байт)

    // Заглушка: количество значащих разрядов - 12
    request[9] = 12; // Байт 9

    // Заглушка: контрольная сумма запроса (алгоритм CRC16)
    unsigned short crc16 = calculate_crc16(request, 10);
    request[10] = (crc16 >> 8) & 0xFF; // Байт 10 (старший байт)
    request[11] = crc16 & 0xFF;        // Байт 11 (младший байт)
}

int main()
{
    int client_socket;
    struct sockaddr_in server_addr;

    // Создание сокета
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Подключение к серверу
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Connection error");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Основной цикл клиента
    while (1)
    {
        unsigned char request[12];
        generate_request(request);

        // Отправка запроса серверу
        if (send(client_socket, request, sizeof(request), 0) == -1)
        {
            perror("Send error");
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        // Получение ответа от сервера
        unsigned char response[32];
        if (recv(client_socket, response, sizeof(response), 0) == -1)
        {
            perror("Receive error");
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        // Обработка ответа от сервера (вывод в консоль)
        printf("Server response: ");
        for (int i = 0; i < 32; i++)
        {
            printf("%d", response[i]);
        }
        printf("\n");

        // Проверка на команду "exit"
        char input_buffer[5];
        printf("Enter 'exit' to quit or any other key to continue: ");
        scanf("%s", input_buffer);

        // Проверка на выход
        if (strcmp(input_buffer, "exit") == 0)
        {
            break;
        }
    }

    // Закрытие сокета
    close(client_socket);

    return 0;
}