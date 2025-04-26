#ifndef CRSF_H
#define CRSF_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#define CRSF_SYNC 0xC8
#define CRSF_MAX_PACKET_LEN 64
#define CRSF_RC_CHANNELS_PACKED 0x16
#define DEFAULT_CONFIG_PATH "/etc/crsf_parser.conf"
#define MAX_CONFIG_LINE_LENGTH 256
#define MAX_SERVICE_NAME_LENGTH 128
#define MAX_LOG_MESSAGE_LENGTH 512

// Уровни логирования
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARNING = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3
} log_level_t;

// Структура для хранения конфигурации
typedef struct {
    int channel_number;
    int threshold;
    char service_name[MAX_SERVICE_NAME_LENGTH];
    int port;
    bool logging_enabled;
    log_level_t log_level;
} crsf_config_t;

// Функция для проверки CRC8
uint8_t crc8_dvb_s2(uint8_t crc, uint8_t a);

// Функция для вычисления CRC8 для массива данных
uint8_t crc8_data(const uint8_t *data, size_t len);

// Функция для распаковки каналов
void unpackCrsfChannels(const uint8_t *data, uint16_t *channels);

// Основная функция для обработки пакетов
int crsf_process_packet(const uint8_t *packet, size_t len, const crsf_config_t *config);

// Функция для чтения конфигурации
int read_config(crsf_config_t *config, const char *config_path);

// Функция для управления systemd сервисом
int control_systemd_service(const char *service_name, bool start);

// Функция для логирования
void crsf_log(const crsf_config_t *config, log_level_t level, const char *format, ...);

#endif // CRSF_H