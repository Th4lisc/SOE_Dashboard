#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <gattlib.h>

#define UUID_TX "0000fff2-0000-1000-8000-00805f9b34fb"
#define UUID_RX "0000fff1-0000-1000-8000-00805f9b34fb"

// Estrutura global para armazenar dados
struct {
    int rpm;
    int speed;
    int temp;
    int fuel;
} obd_data = {0, 0, 0, 0};

// ---------------- BLE SECTION ----------------

void notification_handler(const uuid_t *uuid, const uint8_t *data, size_t len, void *user_data) {
    char resp[128] = {0};
    strncpy(resp, (const char*)data, len);

    if (strstr(resp, "41 0C")) { // RPM
        unsigned int A, B;
        sscanf(resp, "41 0C %x %x", &A, &B);
        obd_data.rpm = ((A * 256) + B) / 4;
    } else if (strstr(resp, "41 0D")) { // Speed
        unsigned int A;
        sscanf(resp, "41 0D %x", &A);
        obd_data.speed = A;
    } else if (strstr(resp, "41 05")) { // Coolant temp
        unsigned int A;
        sscanf(resp, "41 05 %x", &A);
        obd_data.temp = A - 40;
    } else if (strstr(resp, "41 2F")) { // Fuel level
        unsigned int A;
        sscanf(resp, "41 2F %x", &A);
        obd_data.fuel = (A * 100) / 255;
    }
}

void *ble_thread(void *arg) {
    const char addr = (char)arg;
    void *connection;
    uuid_t tx_uuid, rx_uuid;
    int ret;

    connection = gattlib_connect(NULL, addr, "random", "low", 0);
    if (connection == NULL) {
        fprintf(stderr, "Erro ao conectar ao BLE %s\n", addr);
        pthread_exit(NULL);
    }

    gattlib_string_to_uuid(UUID_TX, strlen(UUID_TX), &tx_uuid);
    gattlib_string_to_uuid(UUID_RX, strlen(UUID_RX), &rx_uuid);
    gattlib_register_notification(connection, notification_handler, NULL);
    gattlib_notification_start(connection, &rx_uuid);

    const char *commands[] = {"010C\r", "010D\r", "0105\r", "012F\r"};
    while (1) {
        for (int i = 0; i < 4; i++) {
            gattlib_write_char_by_uuid(connection, &tx_uuid, commands[i], strlen(commands[i]));
            sleep(2);
        }
    }

    gattlib_disconnect(connection);
    pthread_exit(NULL);
}

// ---------------- HTTP SECTION ----------------

void *http_server_thread(void *arg) {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    char buffer[1024];
    char response[1024];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        pthread_exit(NULL);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        pthread_exit(NULL);
    }

    listen(server_fd, 5);
    printf("🌐 Servidor HTTP rodando em http://localhost:8080\n");

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&addr, &addr_len);
        if (client_fd < 0) continue;

        read(client_fd, buffer, sizeof(buffer));

        snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n"
            "{ \"rpm\": %d, \"speed\": %d, \"temperature\": %d, \"fuel\": %d }\r\n",
            obd_data.rpm, obd_data.speed, obd_data.temp, obd_data.fuel);

        write(client_fd, response, strlen(response));
        close(client_fd);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <MAC do ELM327 BLE>\n", argv[0]);
        return 1;
    }

    pthread_t ble_t, http_t;

    pthread_create(&ble_t, NULL, ble_thread, argv[1]);
    pthread_create(&http_t, NULL, http_server_thread, NULL);

    pthread_join(ble_t, NULL);
    pthread_join(http_t, NULL);

    return 0;
}