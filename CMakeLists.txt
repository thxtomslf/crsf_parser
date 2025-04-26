cmake_minimum_required(VERSION 3.10)
project(crsf_parser C)

# Установка стандарта C
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Поиск необходимых пакетов
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

# Добавление исполняемого файла
add_executable(crsf_parser
    main.c
    crsf.c
)

# Установка путей для заголовочных файлов
target_include_directories(crsf_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SYSTEMD_INCLUDE_DIRS}
)

# Установка библиотек для линковки
target_link_libraries(crsf_parser PRIVATE
    ${SYSTEMD_LIBRARIES}
)

# Установка флагов компиляции
target_compile_options(crsf_parser PRIVATE
    -Wall
    -Wextra
    -Werror
)

# Установка путей для установки
install(TARGETS crsf_parser
    RUNTIME DESTINATION bin
)

# Установка конфигурационного файла
install(FILES /etc/crsf_parser.conf
    DESTINATION /etc
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
) 