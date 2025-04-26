#include "crsf.h"
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
#include <systemd/sd-bus.h>
#include <stdarg.h>
#include <time.h>

// Глобальные переменные
static time_t last_threshold_check = 0;

// Функция для логирования
void crsf_log(const crsf_config_t *config, log_level_t level, const char *format, ...) {
    if (!config->logging_enabled || level > config->log_level) {
        return;
    }

    char message[MAX_LOG_MESSAGE_LENGTH];
    char timestamp[32];
    time_t now;
    struct tm *timeinfo;

    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    const char *level_str;
    switch (level) {
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
        case LOG_LEVEL_WARNING: level_str = "WARNING"; break;
        case LOG_LEVEL_INFO: level_str = "INFO"; break;
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        default: level_str = "UNKNOWN"; break;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    fprintf(stderr, "[%s] [%s] %s\n", timestamp, level_str, message);
}

// Функция для чтения конфигурации
int read_config(crsf_config_t *config, const char *config_path) {
    FILE *file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "Failed to open config file: %s\n", config_path);
        return -1;
    }

    // Устанавливаем значения по умолчанию
    config->logging_enabled = true;
    config->log_level = LOG_LEVEL_INFO;

    char line[MAX_CONFIG_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        
        if (!key || !value) continue;
        
        // Удаляем пробелы в начале и конце
        while (*key == ' ') key++;
        while (*value == ' ') value++;
        
        if (strcmp(key, "CHANNEL") == 0) {
            config->channel_number = atoi(value);
        } else if (strcmp(key, "THRESHOLD") == 0) {
            config->threshold = atoi(value);
        } else if (strcmp(key, "SERVICE") == 0) {
            strncpy(config->service_name, value, MAX_SERVICE_NAME_LENGTH - 1);
            config->service_name[MAX_SERVICE_NAME_LENGTH - 1] = '\0';
        } else if (strcmp(key, "PORT") == 0) {
            config->port = atoi(value);
        } else if (strcmp(key, "LOGGING_ENABLED") == 0) {
            config->logging_enabled = (strcasecmp(value, "true") == 0);
        } else if (strcmp(key, "LOG_LEVEL") == 0) {
            if (strcasecmp(value, "ERROR") == 0) config->log_level = LOG_LEVEL_ERROR;
            else if (strcasecmp(value, "WARNING") == 0) config->log_level = LOG_LEVEL_WARNING;
            else if (strcasecmp(value, "INFO") == 0) config->log_level = LOG_LEVEL_INFO;
            else if (strcasecmp(value, "DEBUG") == 0) config->log_level = LOG_LEVEL_DEBUG;
        }
    }

    fclose(file);
    return 0;
}

// Функция для управления systemd сервисом
int control_systemd_service(const char *service_name, bool start) {
    sd_bus *bus = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;

    // Подключаемся к системной шине
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        return r;
    }

    // Формируем имя сервиса
    char unit_name[256];
    snprintf(unit_name, sizeof(unit_name), "%s.service", service_name);

    // Выполняем команду start/stop
    if (start) {
        r = sd_bus_call_method(bus,
                              "org.freedesktop.systemd1",
                              "/org/freedesktop/systemd1",
                              "org.freedesktop.systemd1.Manager",
                              "StartUnit",
                              &error,
                              &reply,
                              "ss",
                              unit_name,
                              "replace");
    } else {
        r = sd_bus_call_method(bus,
                              "org.freedesktop.systemd1",
                              "/org/freedesktop/systemd1",
                              "org.freedesktop.systemd1.Manager",
                              "StopUnit",
                              &error,
                              &reply,
                              "ss",
                              unit_name,
                              "replace");
    }

    if (r < 0) {
        fprintf(stderr, "Failed to %s service: %s\n", start ? "start" : "stop", error.message);
    }

    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return r;
}

// Функция для проверки CRC8
uint8_t crc8_dvb_s2(uint8_t crc, uint8_t a) {
    crc ^= a;
    for (int ii = 0; ii < 8; ii++) {
        if (crc & 0x80) {
            crc = (crc << 1) ^ 0xD5;
        } else {
            crc = crc << 1;
        }
    }
    return crc;
}

// Функция для вычисления CRC8 для массива данных
uint8_t crc8_data(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc = crc8_dvb_s2(crc, data[i]);
    }
    return crc;
}

// Функция для распаковки каналов
void unpackCrsfChannels(const uint8_t *data, uint16_t *channels) {
    int bit_offset = 0;
    int byte_offset = 0;

    for (int i = 0; i < 16; i++) {
        int bits_remaining = 11;
        uint16_t channel_value = 0;

        while (bits_remaining > 0) {
            int bits_from_current_byte = (8 - bit_offset) < bits_remaining ? (8 - bit_offset) : bits_remaining;
            uint8_t mask = (1 << bits_from_current_byte) - 1;
            channel_value |= ((data[byte_offset] >> bit_offset) & mask) << (11 - bits_remaining);

            bits_remaining -= bits_from_current_byte;
            bit_offset += bits_from_current_byte;

            if (bit_offset >= 8) {
                bit_offset = 0;
                byte_offset++;
            }
        }

        channels[i] = channel_value;
    }
}

// Функция для обработки пакетов
int crsf_process_packet(const uint8_t *packet, size_t len, const crsf_config_t *config) {
    // Проверка длины пакета
    if (len < 4 || len > CRSF_MAX_PACKET_LEN) {
        crsf_log(config, LOG_LEVEL_ERROR, "Invalid packet length: %zu", len);
        return -1;
    }

    // Проверка типа пакета
    if (packet[2] != CRSF_RC_CHANNELS_PACKED) {
        crsf_log(config, LOG_LEVEL_ERROR, "Invalid packet type: 0x%02X", packet[2]);
        return -2;
    }

    // Проверка CRC
    uint8_t crc = crc8_data(packet + 2, len - 3);
    if (crc != packet[len - 1]) {
        crsf_log(config, LOG_LEVEL_ERROR, "CRC check failed: expected 0x%02X, got 0x%02X", crc, packet[len - 1]);
        return -3;
    }

    // Распаковка каналов
    uint16_t channels[16];
    unpackCrsfChannels(packet + 3, channels);

    // Проверка порога для указанного канала (не чаще чем раз в секунду)
    time_t now = time(NULL);
    if (now - last_threshold_check >= 1) {
        last_threshold_check = now;

        uint16_t channel_value = channels[config->channel_number - 1];
        crsf_log(config, LOG_LEVEL_DEBUG, "Channel %d value: %d", config->channel_number, channel_value);

        if (channel_value > config->threshold) {
            crsf_log(config, LOG_LEVEL_INFO, "Channel value %d exceeds threshold %d, starting service %s", 
                    channel_value, config->threshold, config->service_name);
            control_systemd_service(config->service_name, true);
        } else {
            crsf_log(config, LOG_LEVEL_INFO, "Channel value %d below threshold %d, stopping service %s", 
                    channel_value, config->threshold, config->service_name);
            control_systemd_service(config->service_name, false);
        }
    }

    return 0;
}