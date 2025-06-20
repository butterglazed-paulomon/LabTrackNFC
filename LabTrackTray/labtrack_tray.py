import json
import threading
import requests
import csv
import os
import signal
from flask import Flask, request, jsonify
from datetime import datetime
from win10toast import ToastNotifier
import tkinter as tk
from tkinter import messagebox
from pystray import Icon, Menu, MenuItem
from PIL import Image, ImageDraw

# Load config
with open('config.json', 'r') as f:
    config = json.load(f)

LOG_FILE = "log.csv"
WEBHOOK_URL = config["webhook_url"]
HOST = "0.0.0.0"
PORT = 8000

app = Flask(__name__)
notifier = ToastNotifier()
flask_thread = None
icon = None
server_running = False
server_should_run = True

@app.route("/receive", methods=["POST"])
def receive():
    try:
        data = request.get_json(force=True)
        uid = data.get("uid", "").strip()
        action = data.get("action", "").strip().lower()

        if not uid or action not in ["borrow", "return"]:
            return jsonify({"error": "Invalid data"}), 400

        print(f"Received: UID={uid}, Action={action}")

        with open(LOG_FILE, 'a', newline='') as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow([datetime.now().isoformat(), uid, action])

        response = requests.get(WEBHOOK_URL, params={"uid": uid, "action": action}, timeout=5)
        print(f"Forwarded to webhook, status: {response.status_code}")

        notifier.show_toast("LabTrack NFC", f"{action.upper()} UID: {uid}", duration=3, threaded=True)

        return jsonify({"status": "ok"}), 200

    except Exception as e:
        print(f"Error: {e}")
        return jsonify({"error": "Server error"}), 500

def run_flask():
    global server_running
    server_running = True
    app.run(host=HOST, port=PORT)
    server_running = False

def start_server():
    global flask_thread, server_running
    if not server_running:
        flask_thread = threading.Thread(target=run_flask, daemon=True)
        flask_thread.start()
        print("Flask server started.")

def stop_server():
    global server_running
    if server_running:
        os.kill(os.getpid(), signal.SIGTERM)  # Force kill the app (safe because we're daemonized)
        print("Flask server stopped.")

def open_log():
    if not os.path.exists(LOG_FILE):
        with open(LOG_FILE, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(["timestamp", "uid", "action"])
    os.system(f'notepad {LOG_FILE}')

def create_image():
    image = Image.new('RGB', (64, 64), color='white')
    d = ImageDraw.Draw(image)
    d.rectangle([16, 16, 48, 48], fill='black')
    return image

def setup_tray():
    global icon
    menu = Menu(
        MenuItem('Start Server', lambda: start_server()),
        MenuItem('Stop Server', lambda: stop_server()),
        MenuItem('View Log', lambda: open_log()),
        MenuItem('Quit', lambda: icon.stop())
    )
    icon = Icon("LabTrack", create_image(), "LabTrack NFC", menu)
    start_server()
    icon.run()

def run_gui_minimized():
    root = tk.Tk()
    root.withdraw()  # Hide the window, app runs from tray
    setup_tray()

if __name__ == "__main__":
    run_gui_minimized()
