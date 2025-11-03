import subprocess
import sys
import os
import time

# --- Конфигурация путей ---
# Получаем абсолютный путь к директории, где находится этот скрипт
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# Определяем пути к скриптам и их рабочим директориям
ANALYZE_ENGINE_DIR = os.path.join(BASE_DIR, 'analyze_engine')
ANALYZE_SCRIPT = 'main.py'

# --- Конфигурация запуска ---
DJANGO_SERVER_ADDR = "127.0.0.1:8000"  # Адрес и порт для Django-сервера

DASHBOARD_DIR = os.path.join(ANALYZE_ENGINE_DIR, 'dashboard')
DJANGO_SCRIPT = 'manage.py'


def main():
    """
    Запускает Django-сервер и скрипт анализатора как два отдельных процесса.
    Ожидает прерывания (Ctrl+C) для их корректного завершения.
    """
    processes = []
    # Используем тот же интерпретатор Python, которым запущен этот скрипт
    python_executable = sys.executable

    print("--- Запуск сервисов ---")

    try:
        # Запускаем скрипт анализатора (main.py)
        print(f"Запуск анализатора: {ANALYZE_SCRIPT} в {ANALYZE_ENGINE_DIR}")
        analyzer_process = subprocess.Popen(
            [python_executable, ANALYZE_SCRIPT],
            cwd=ANALYZE_ENGINE_DIR
        )
        processes.append(analyzer_process)

        # Запускаем Django-сервер
        print(f"Запуск Django-дашборда на http://{DJANGO_SERVER_ADDR}")
        django_process = subprocess.Popen(
            [python_executable, DJANGO_SCRIPT, 'runserver', DJANGO_SERVER_ADDR],
            cwd=DASHBOARD_DIR
        )
        processes.append(django_process)

        # Ожидаем, пока пользователь не прервет скрипт (Ctrl+C)
        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        print("\n--- Получен сигнал завершения (Ctrl+C). Остановка сервисов... ---")
    finally:
        for proc in processes:
            proc.terminate()  # Отправляем сигнал завершения каждому процессу
        print("Все сервисы остановлены.")


if __name__ == "__main__":
    main()