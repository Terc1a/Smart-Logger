from openai import OpenAI
import mysql.connector
from datetime import datetime, timedelta, timezone
import schedule
import time
import yaml
import logging

logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

logging.debug("Loading configuration from config.yaml...")
with open('config.yaml', 'r', encoding='utf-8') as f:
    config = yaml.safe_load(f)
logging.debug("Configuration loaded successfully.")

def get_data():
    """Подключается к БД, получает слова за последние 10 минут и закрывает соединение."""
    logging.debug("Executing get_data()...")
    current_datetime = datetime.now(timezone.utc)
    past_datetime = current_datetime - timedelta(minutes=10)
    final = []
    connection = None  # Инициализируем переменную
    try:
        connection = mysql.connector.connect(**config['database'])
        cursor = connection.cursor()
        logging.debug("Database connection successful in get_data.")
        query = "SELECT word FROM words WHERE timestamp >= %s"
        cursor.execute(query, (past_datetime,))
        results = cursor.fetchall()
        logging.debug(f"Found {len(results)} words in the last 10 minutes.")
        for row in results:
            final.append(row[0])
    except mysql.connector.Error as err:
        logging.error(f"Error in get_data: {err}")
    finally:
        if connection and connection.is_connected():
            connection.close()
            logging.debug("Database connection closed in get_data.")
    return final


def send_prompt():
    """Получает данные, отправляет промпт и записывает результат в БД."""
    logging.info("Starting send_prompt job...")
    current_datetime = datetime.now(timezone.utc)
    # Отправка промпта нейросети
    word_data = get_data()
    if not word_data:
        logging.info("No new words found. Skipping LLM prompt.")
        return

    # Форматируем промпт, подставляя в него данные
    prompt = config['llm']['prompt'].format(word_data=word_data)
    client = OpenAI(
        base_url="http://localhost:1234/v1",  # Адрес LM Studio
        api_key="lm-studio"                   # Для LM Studio ключ может быть любым, но лучше указать "lm-studio"
    )

    logging.debug("Sending prompt to LLM...")
    response = client.chat.completions.create(
        model="liquid/lfm2-1.2b",  # Укажите имя модели, как оно отображается в LM Studio
        messages=[{"role": "user", "content": prompt}]
    )
    result_content = response.choices[0].message.content

    connection = None # Инициализируем переменную
    try:
        connection = mysql.connector.connect(**config['database'])
        logging.debug("Database connection successful in send_prompt.")
        cursor = connection.cursor()
        query = "INSERT INTO mentality(state, timestamp) VALUES (%s,%s)"
        vals = (result_content, current_datetime)
        cursor.execute(query, vals)
        connection.commit()
        logging.debug(f"Successfully inserted state '{result_content}' into database.")
    except mysql.connector.Error as err:
        logging.error(f"Error in send_prompt: {err}")
    finally:
        if connection and connection.is_connected():
            connection.close()
            logging.debug("Database connection closed in send_prompt.")

    logging.info(f"Saved state: {result_content} at {current_datetime.strftime('%Y-%m-%d %H:%M:%S')}")

schedule.every(1).minutes.do(send_prompt)
logging.info(f"Scheduler started at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}. Waiting for the first job...")
while True:
    schedule.run_pending()
    time.sleep(1)

