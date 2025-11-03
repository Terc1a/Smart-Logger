from openai import OpenAI
import mysql.connector
from datetime import datetime, timedelta, timezone
import schedule
import time

def get_data():
    """Подключается к БД, получает слова за последние 10 минут и закрывает соединение."""
    current_datetime = datetime.now(timezone.utc)
    past_datetime = current_datetime - timedelta(minutes=10)
    final = []
    connection = None  # Инициализируем переменную
    try:
        connection = mysql.connector.connect(
            host="192.168.0.100",
            user="logger",
            password="qn9@NiUYb",
            database="keylogging"
        )
        cursor = connection.cursor()
        query = "SELECT word FROM words WHERE timestamp >= %s"
        cursor.execute(query, (past_datetime,))
        results = cursor.fetchall()
        for row in results:
            final.append(row[0])
    except mysql.connector.Error as err:
        print(f"Error: {err}")
    finally:
        if connection and connection.is_connected():
            connection.close()
    return final


def send_prompt():
    """Получает данные, отправляет промпт и записывает результат в БД."""
    current_datetime = datetime.now(timezone.utc)
    # Отправка промпта нейросети
    word_data = get_data()
    prompt = f"Ты проводишь анализ сентимента и эмоционального окраса текста. Перед тобой текст, разделенный по словам, проанализируй его указанным выше методом и предоставь вердикт, какое настроение было у человека в момент его написания. На выбор у тебя несколько вариантов:\n1. Хорошее\n2. Грустное\n3. Смешанное\n4. Злость\n5. Обида\nСам текст:\n{word_data}\nОтвечай СТРОГО одним из этих вариантов, одним словом, и не пиши СОВСЕМ ничего кроме одного слова.\nУчитывай, что пользователь может допускать опечатки в словах"
    client = OpenAI(
        base_url="http://localhost:1234/v1",  # Адрес LM Studio
        api_key="lm-studio"                   # Для LM Studio ключ может быть любым, но лучше указать "lm-studio"
    )

    response = client.chat.completions.create(
        model="liquid/lfm2-1.2b",  # Укажите имя модели, как оно отображается в LM Studio
        messages=[{"role": "user", "content": prompt}]
    )
    result_content = response.choices[0].message.content

    connection = None # Инициализируем переменную
    try:
        connection = mysql.connector.connect(
            host="192.168.0.100",
            user="logger",
            password="qn9@NiUYb",
            database="keylogging"
        )
        cursor = connection.cursor()
        query = "INSERT INTO mentality(state, timestamp) VALUES (%s,%s)"
        vals = (result_content, current_datetime)
        cursor.execute(query, vals)
        connection.commit()
    except mysql.connector.Error as err:
        print(f"Error in send_prompt: {err}")
    finally:
        if connection and connection.is_connected():
            connection.close()

    print(f"Saved state: {result_content} at {current_datetime.strftime('%Y-%m-%d %H:%M:%S')}")

schedule.every(1).minutes.do(send_prompt)
print("Start pending", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
while True:
    schedule.run_pending()
    time.sleep(1)

# Настроение, текущая активность(занятие, например "пишет код"), общее состояние(бодрый, уставший, и тд), топ слов

# SELECT `word`, COUNT(*) AS `количество`
# FROM `words`
# WHERE CHAR_LENGTH(`word`) >= 3
# GROUP BY `word`
# HAVING COUNT(*) >= 2
# ORDER BY CHAR_LENGTH(`word`) DESC, COUNT(*) DESC;