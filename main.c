#include "crsf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -c, --config PATH    Path to configuration file (default: %s)\n", DEFAULT_CONFIG_PATH);
    printf("  -h, --help          Show this help message\n");
}

int main(int argc, char *argv[]) {
    const char *config_path = DEFAULT_CONFIG_PATH;
    
    // Обработка аргументов командной строки
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                config_path = argv[++i];
            } else {
                fprintf(stderr, "Error: --config requires a path argument\n");
                print_usage(argv[0]);
                return 1;
            }
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    crsf_config_t config;
    
    // Читаем конфигурацию из файла
    if (read_config(&config, config_path) != 0) {
        fprintf(stderr, "Failed to read configuration from %s\n", config_path);
        return 1;
    }

    crsf_log(&config, LOG_LEVEL_INFO, "Starting CRSF parser with config from %s", config_path);

    // Создание UDP сокета
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        crsf_log(&config, LOG_LEVEL_ERROR, "Failed to create socket: %s", strerror(errno));
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config.port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        crsf_log(&config, LOG_LEVEL_ERROR, "Failed to bind socket: %s", strerror(errno));
        close(sockfd);
        return 1;
    }

    crsf_log(&config, LOG_LEVEL_INFO, "Listening on port %d", config.port);

    uint8_t input_buffer[CRSF_MAX_PACKET_LEN];

    while (1) {
        // Чтение данных из сокета
        ssize_t len = recv(sockfd, input_buffer, CRSF_MAX_PACKET_LEN, 0);
        if (len < 0) {
            crsf_log(&config, LOG_LEVEL_ERROR, "Failed to receive data: %s", strerror(errno));
            break;
        }

        // Обработка пакета
        int result = crsf_process_packet(input_buffer, len, &config);
        if (result != 0) {
            crsf_log(&config, LOG_LEVEL_ERROR, "Packet processing failed with code %d", result);
        }
    }

    close(sockfd);
    return 0;
}