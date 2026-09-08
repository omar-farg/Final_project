import os
import asyncio
import telegram
import serial  # for serial communication with the stm
import joblib  # for loading the trained model
import time  # for delaying the time so the stm connects to the laptop
import tkinter as tk  # for creating the GUI
from PIL import Image, ImageTk  # to show the gas alert image in the GUI
import sounddevice as sd  # to recored the baby cry sound
from scipy.io.wavfile import write

# global counter to record the cry sound every 3 seconds
cry_record_counter = 0
BOT_TOKEN = "8813866378:AAE9nWl1VmceHBrfdyB24GAEXXlKl3x4stg"  # tTelegram chat Bot token
CHAT_ID = "1148690625"


async def send_telegram_alert(message):
    bot = telegram.Bot(token=BOT_TOKEN)
    await bot.send_message(chat_id=CHAT_ID, text=message)


# establishing the connectoin between python and the stm
try:
    stm = serial.Serial(
        "COM3", 9600, timeout=0.1
    )  #'COM3' is the port number and it we will know it from the arduino ide or the device manager in the widows
    time.sleep(2)  # waits 2 sec so the stm connects and the usb drivers work proberly
    script_dir = os.path.dirname(os.path.abspath(__file__))
    pkl_path = os.path.join(script_dir, "cry_classifier.pkl")
    classifier = joblib.load(pkl_path)
except Exception as e:
    print(f"WARNING Error occured:{e}")
    stm = None
    classifier = None

# building the gui
root = tk.Tk()
root.title("Smart Nursery Gurdian")
root.geometry("800x600")
status_label = tk.Label(
    root, text="No Gas detected", font=("Times New Roman", 18), fg="green"
)
status_label.pack(pady=20)
# to monitor temp
temp_label = tk.Label(
    root, text="Temperature:--°C", font=("Times New Roman", 18), fg="black"
)
temp_label.pack(pady=5)
# creating image label so it`s ready to display it when there is gas
image_label = tk.Label(root)
image_label.pack(pady=10)
try:
    alert_image = Image.open("gas_alert.png")
    alert_image = alert_image.resize((800, 400))
    gas_alert = ImageTk.PhotoImage(alert_image)

except Exception as e:
    print(f"loading image failed: {e}")
    gas_alert = None


# for caputring the cry sound
def record_cry(file_name="cry_record.wav", duration=3, sample_rate=16000):
    try:
        record = sd.rec(
            int(sample_rate * duration),
            samplerate=sample_rate,
            channels=1,
            dtype="float32",
        )
        sd.wait()
        write(file_name, sample_rate, record)
        return file_name
    except Exception as e:
        print(f"Error with recording the file:{e}")
        return None


# making the model process the audio
def audio_process(audio_file_path):
    if classifier is None:
        return
    try:
        prediction = str(classifier.predict(audio_file_path)).lower().strip()
        print(prediction)
        if prediction == "no cry detected":
            if stm:
                stm.write(b"CRY:OFF\n")  # sends to the stm that there is no cry
            return
        # if there is crying
        # .replace()is for if there is message like belly_pain it will be Belly Pain
        #  .title() capitalize 1st letter in every word
        crying_message = prediction.replace("_", " ").title()
        status_label.config(
            text=f"Baby Crying Alert!\nReason: {crying_message}",
            font=("Times New Roman", 22),
            fg="red",
        )
        # sends to the stm that there is crying
        if stm:
            stm.write(b"CRY:ON\n")
    except Exception as e:
        print(f"Audio processing error: {e}")
        return


def process_system():
    global cry_record_counter
    while stm and stm.in_waiting > 0:
        try:
            receievd_message = stm.readline().decode("utf-8").strip()
            if receievd_message == "ALERT:GAS DETECTED":
                status_label.config(text="")
                temp_label.config(text="")
                asyncio.run(send_telegram_alert("Gas detected"))
                if gas_alert:
                    image_label.config(image=gas_alert)
                    image_label.image = gas_alert
            elif receievd_message == "STATUS:GAS CLEAR":
                image_label.config(image="")
                status_label.config(text="No Gas detected", fg="green")
            elif receievd_message == "BABY AWAKE ALERT!":
                status_label.config(
                    text="Baby Awake", font=("Times New Roman", 22), fg="red"
                )
            elif receievd_message.startswith(
                "temperature:"
            ):  # Matches exact case + colon
                parts = receievd_message.split(":")
                if len(parts) > 1:  # verifies index [1] actually exists
                    temp_label.config(text=f"Temperature: {parts[1]}°C")
        except UnicodeDecodeError:
            pass

        # recording the audio every 3 seconds
    cry_record_counter += 1
    if cry_record_counter >= 30:
        cry_record_counter = 0
        cry_file = record_cry(duration=3)
        if cry_file:
            audio_process(cry_file)
    root.after(100, process_system)


# start the app loop
root.after(100, process_system)
root.mainloop()
