cmake_minimum_required(VERSION 3.10)
project(keylogger C)

set(CMAKE_C_STANDARD 11)

# --- Явное указание путей к библиотекам MySQL Server ---
# 1. Указываем путь к корневой папке MySQL Server.
set(MYSQL_SERVER_PATH "C:/Program Files/MySQL/MySQL Server 8.0")

add_executable(keylogger main.c)

# 2. Добавляем путь к заголовочным файлам (.h)
target_include_directories(keylogger PRIVATE "${MYSQL_SERVER_PATH}/include")

# 3. Прилинковываем библиотеку (.lib)
target_link_libraries(keylogger PRIVATE "${MYSQL_SERVER_PATH}/lib/libmysql.lib")

# 4. Копируем DLL в папку со сборкой, чтобы .exe мог ее найти при запуске.
add_custom_command(TARGET keylogger POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MYSQL_SERVER_PATH}/lib/libmysql.dll"
    $<TARGET_FILE_DIR:keylogger>
)

# 5. Копируем зависимые DLL от OpenSSL, которые нужны для libmysql.dll
add_custom_command(TARGET keylogger POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MYSQL_SERVER_PATH}/bin/libcrypto-3-x64.dll"
    "${MYSQL_SERVER_PATH}/bin/libssl-3-x64.dll"
    $<TARGET_FILE_DIR:keylogger>
)
