from flask import Flask, request, render_template_string, redirect, url_for

app = Flask(__name__)

# ==========================================
# 1. BAZA DANYCH (Tymczasowa symulacja)
# Docelowo ten fragment zostanie zastąpiony zapytaniami do API WhatsAppa
# ==========================================
CHATS = {
    "48123456789": {
        "name": "Kumpel z pracy",
        "messages": [
            {"sender": "Kumpel z pracy", "text": "Hej, idziemy na piwo?"},
            {"sender": "Ja", "text": "Jasne, o 20:00?"}
        ]
    },
    "48987654321": {
        "name": "Mama",
        "messages": [
            {"sender": "Mama", "text": "Kupiles chleb?"}
        ]
    }
}

# ==========================================
# 2. SZABLONY HTML (Dla Nokii E75)
# Ultra-prosty kod, bez CSS i JS, ładuje się w ułamek sekundy
# ==========================================
TEMPLATE_INDEX = """
<html>
<head><title>WA Bramka</title></head>
<body style="font-family: sans-serif;">
    <h2>💬 Moje Czaty</h2>
    <hr>
    {% for chat_id, chat_data in chats.items() %}
        <p><a href="/chat/{{ chat_id }}">📁 {{ chat_data.name }}</a></p>
    {% endfor %}
    <hr>
    <small><a href="/">[Odswiez liste]</a></small>
</body>
</html>
"""

TEMPLATE_CHAT = """
<html>
<head><title>Czat: {{ chat_name }}</title></head>
<body style="font-family: sans-serif;">
    <h3>👤 {{ chat_name }}</h3>
    <a href="/">[< Wroc do listy]</a>
    <hr>
    {% for msg in messages %}
        <p><b>{{ msg.sender }}:</b> {{ msg.text }}</p>
    {% endfor %}
    <hr>
    <form action="/send/{{ chat_id }}" method="POST">
        <input type="text" name="message" size="25" placeholder="Napisz wiadomosc...">
        <br><br>
        <input type="submit" value="Wyslij">
    </form>
    <a href="/chat/{{ chat_id }}">[Odswiez czat]</a>
</body>
</html>
"""

# ==========================================
# 3. LOGIKA APLIKACJI (Routing)
# ==========================================

@app.route('/')
def index():
    # Wyświetla listę wszystkich czatów
    return render_template_string(TEMPLATE_INDEX, chats=CHATS)

@app.route('/chat/<chat_id>')
def view_chat(chat_id):
    # Wyświetla konkretny czat
    if chat_id not in CHATS:
        return "Czat nie istnieje! <a href='/'>Wroc</a>"

    chat_data = CHATS[chat_id]
    return render_template_string(TEMPLATE_CHAT,
                                  chat_id=chat_id,
                                  chat_name=chat_data['name'],
                                  messages=chat_data['messages'])

@app.route('/send/<chat_id>', methods=['POST'])
def send_message(chat_id):
    # Odbiera wiadomość z Nokii i dodaje do czatu
    if chat_id in CHATS:
        new_text = request.form.get('message')
        if new_text:
            # Docelowo tutaj wysyłamy komendę do prawdziwego WhatsAppa
            CHATS[chat_id]['messages'].append({"sender": "Ja", "text": new_text})
            print(f"--> Wysłano do {chat_id}: {new_text}")

    # Po wysłaniu wracamy do tego samego czatu
    return redirect(url_for('view_chat', chat_id=chat_id))

if __name__ == '__main__':
    # Uruchamia serwer na porcie 80 (standardowy port WWW)
    # Możesz zmienić na 5000, jeśli port 80 jest zajęty na Twoim serwerze.
    app.run(host='0.0.0.0', port=80)