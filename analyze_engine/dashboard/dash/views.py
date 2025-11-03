from django.shortcuts import render
from django.conf import settings
import mysql.connector
import yaml

CONFIG_FILE_PATH = settings.BASE_DIR.parent / 'config.yaml'
with open(CONFIG_FILE_PATH, 'r', encoding='utf-8') as f:
    config = yaml.safe_load(f)
def index(request):
    emotes = []
    most_usage = []
    histogram_data = []
    connection = None
    try:
        connection = mysql.connector.connect(**config['database'])
        cursor = connection.cursor()

        query = "SELECT `state`, `timestamp` FROM (SELECT `state`, `timestamp` FROM `mentality` ORDER BY `timestamp` DESC LIMIT 10) AS last_10 ORDER BY `timestamp` ASC"
        cursor.execute(query)
        emotes = cursor.fetchall()

        query_words = "SELECT `word`, COUNT(*) AS `count` FROM `words` WHERE CHAR_LENGTH(`word`) >= 3 GROUP BY `word` HAVING COUNT(*) >= 2 ORDER BY CHAR_LENGTH(`word`) DESC, COUNT(*) DESC LIMIT 15"
        cursor.execute(query_words)
        most_usage = cursor.fetchall()

        # Cборка данных для гистограммы
        max_count = max((count for word, count in most_usage), default=1)
        for word, count in most_usage:
            # Вычисляем ширину полоски в процентах
            bar_width_percent = round((count / max_count) * 100, 2)
            histogram_data.append({'word': word, 'count': count, 'bar_width_percent': bar_width_percent})

    except mysql.connector.Error as err:
        print(f"Database Error: {err}")
    finally:
        if connection and connection.is_connected():
            connection.close()

        context = {"emotes": emotes, "most_usage": most_usage, "histogram_data": histogram_data}
    return render(request, "dash/index.html", context)