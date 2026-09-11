import asyncio
import os
import serial
import time
import tkinter as tk
from PIL import Image, ImageTk
import joblib
import sounddevice as sd
from scipy.io.wavfile import write
import telegram
from telegram.request import HTTPXRequest
import __main__

from cry_classifier import CryClassifier


# ============================================================
# FIX FOR PICKLE / JOBLIB CryClassifier
# ============================================================

__main__.CryClassifier = CryClassifier


# ============================================================
# PATHS
# ============================================================

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

TEST_CRY_FILE = os.path.join(
    SCRIPT_DIR,
    "test.wav"
)

MODEL_FILE = os.path.join(
    SCRIPT_DIR,
    "cry_classifier.pkl"
)

GAS_IMAGE_FILE = os.path.join(
    SCRIPT_DIR,
    "gas_alert.png"
)


# ============================================================
# TELEGRAM
# ============================================================


BOT_TOKEN = "8813866378:AAHbe6_5GPZG7p8BMADorn-h8bF5iN2KVuY"
CHAT_ID = "1148690625"


# ============================================================
# STM32 SERIAL
# ============================================================

STM_PORT = "COM7"
STM_BAUDRATE = 9600


# ============================================================
# GLOBAL VARIABLES
# ============================================================

cry_record_counter = 0

stm = None
classifier = None
gas_alert = None


# ============================================================
# TELEGRAM FUNCTION
# ============================================================

async def send_telegram_alert(message):

    try:

        print()
        print("================================")
        print("SENDING TELEGRAM ALERT")
        print("================================")

        print("Message:")
        print(message)

        request = HTTPXRequest(
            connect_timeout=20,
            read_timeout=20
        )

        bot = telegram.Bot(
            token=BOT_TOKEN,
            request=request
        )

        await bot.send_message(
            chat_id=CHAT_ID,
            text=message
        )

        print("Telegram alert sent successfully.")

    except Exception as e:

        print()
        print("TELEGRAM ERROR")
        print("ERROR TYPE:", type(e).__name__)
        print("ERROR:", e)


def send_telegram(message):

    try:

        asyncio.run(
            send_telegram_alert(message)
        )

    except Exception as e:

        print()
        print("TELEGRAM EXECUTION ERROR")
        print("ERROR TYPE:", type(e).__name__)
        print("ERROR:", e)


# ============================================================
# CONNECT TO STM32
# ============================================================

print()
print("========================================")
print("CONNECTING TO STM32")
print("========================================")

try:

    stm = serial.Serial(
        port=STM_PORT,
        baudrate=STM_BAUDRATE,
        timeout=0.1
    )

    time.sleep(2)

    print("STM32 connected successfully.")
    print("Port:", stm.port)
    print("Baudrate:", stm.baudrate)

except Exception as e:

    print("STM32 CONNECTION ERROR")
    print("ERROR TYPE:", type(e).__name__)
    print("ERROR:", e)

    stm = None


# ============================================================
# LOAD CRY CLASSIFIER
# ============================================================

print()
print("========================================")
print("LOADING CRY CLASSIFIER")
print("========================================")

print("Model path:")
print(MODEL_FILE)

if not os.path.isfile(MODEL_FILE):

    print()
    print("ERROR: cry_classifier.pkl was NOT found.")
    print("Expected location:")
    print(MODEL_FILE)

    classifier = None

else:

    print("Model file found.")

    try:

        classifier = joblib.load(
            MODEL_FILE
        )

        print("Cry classifier loaded successfully.")
        print("Classifier type:")
        print(type(classifier))

    except Exception as e:

        print()
        print("CLASSIFIER LOAD ERROR")
        print("ERROR TYPE:", type(e).__name__)
        print("ERROR:", e)

        classifier = None


# ============================================================
# GUI
# ============================================================

root = tk.Tk()

root.title(
    "Smart Nursery Guardian"
)

root.geometry(
    "800x600"
)


# ============================================================
# CRY STATUS
# ============================================================

status_label = tk.Label(
    root,
    text="No Cry Detected",
    font=("Times New Roman", 22),
    fg="green"
)

status_label.pack(
    pady=20
)


# ============================================================
# MOTION STATUS
# ============================================================

motion_label = tk.Label(
    root,
    text="Motion: SLEEPING",
    font=("Times New Roman", 22),
    fg="green"
)

motion_label.pack(
    pady=5
)


# ============================================================
# TEMPERATURE
# ============================================================

temp_label = tk.Label(
    root,
    text="Temperature: --°C",
    font=("Times New Roman", 22),
    fg="black"
)

temp_label.pack(
    pady=5
)


# ============================================================
# GAS IMAGE
# ============================================================

image_label = tk.Label(
    root
)

image_label.pack(
    pady=10
)


# ============================================================
# LOAD GAS IMAGE
# ============================================================

try:

    if os.path.isfile(GAS_IMAGE_FILE):

        print()
        print("Loading gas alert image...")

        alert_image = Image.open(
            GAS_IMAGE_FILE
        )

        alert_image = alert_image.resize(
            (800, 400)
        )

        gas_alert = ImageTk.PhotoImage(
            alert_image
        )

        print(
            "Gas alert image loaded successfully."
        )

    else:

        print()
        print(
            "WARNING: gas_alert.png was not found."
        )

except Exception as e:

    print(
        "Gas image loading error:"
    )

    print(
        "ERROR TYPE:",
        type(e).__name__
    )

    print(
        "ERROR:",
        e
    )


# ============================================================
# RECORD BABY CRY
# ============================================================

def record_cry(
    file_name="live_cry.wav",
    duration=3,
    sample_rate=16000
):

    try:

        print()
        print("================================")
        print("RECORDING BABY CRY")
        print("================================")

        print(
            "Duration:",
            duration,
            "seconds"
        )

        print(
            "Sample rate:",
            sample_rate
        )

        record = sd.rec(
            int(
                sample_rate * duration
            ),
            samplerate=sample_rate,
            channels=1,
            dtype="float32"
        )

        sd.wait()

        file_path = os.path.join(
            SCRIPT_DIR,
            file_name
        )

        write(
            file_path,
            sample_rate,
            record
        )

        print(
            "Recording saved:"
        )

        print(
            file_path
        )

        return file_path

    except Exception as e:

        print()
        print("RECORDING ERROR")

        print(
            "ERROR TYPE:",
            type(e).__name__
        )

        print(
            "ERROR:",
            e
        )

        return None


# ============================================================
# PROCESS AUDIO FILE WITH MODEL
# ============================================================

def audio_process(
    audio_file_path
):

    print()
    print("================================")
    print("PROCESSING AUDIO")
    print("================================")

    # --------------------------------------------------------
    # CHECK CLASSIFIER
    # --------------------------------------------------------

    if classifier is None:

        print(
            "ERROR: Classifier is not loaded."
        )

        print(
            "Cannot process audio."
        )

        return

    # --------------------------------------------------------
    # CHECK AUDIO FILE
    # --------------------------------------------------------

    if not os.path.isfile(
        audio_file_path
    ):

        print(
            "ERROR: Audio file not found:"
        )

        print(
            audio_file_path
        )

        return

    try:

        print(
            "Audio file:"
        )

        print(
            audio_file_path
        )

        # ----------------------------------------------------
        # SEND FILE PATH DIRECTLY TO CryClassifier
        # ----------------------------------------------------

        prediction_result = classifier.predict(
            audio_file_path
        )

        print(
            "Raw model output:"
        )

        print(
            prediction_result
        )

        # ----------------------------------------------------
        # EXTRACT PREDICTION
        # ----------------------------------------------------

        if isinstance(
            prediction_result,
            (list, tuple)
        ):

            if len(
                prediction_result
            ) > 0:

                prediction = str(
                    prediction_result[0]
                )

            else:

                prediction = ""

        elif hasattr(
            prediction_result,
            "tolist"
        ):

            converted_result = (
                prediction_result.tolist()
            )

            if isinstance(
                converted_result,
                list
            ):

                if len(
                    converted_result
                ) > 0:

                    prediction = str(
                        converted_result[0]
                    )

                else:

                    prediction = ""

            else:

                prediction = str(
                    converted_result
                )

        else:

            prediction = str(
                prediction_result
            )

        # ----------------------------------------------------
        # CLEAN MODEL OUTPUT
        # ----------------------------------------------------

        prediction = prediction.strip()

        prediction = prediction.lower()

        prediction = prediction.strip(
            "[]'\" "
        )

        print(
            "Final model prediction:"
        )

        print(
            prediction
        )

        # ====================================================
        # NO CRY
        # ====================================================

        if prediction == "no cry detected":

            status_label.config(
                text="No Cry Detected",
                font=("Times New Roman", 22),
                fg="green"
            )

            if stm is not None:

                try:

                    stm.write(
                        b"CRY:OFF\n"
                    )

                    print(
                        "Sent to STM32: CRY:OFF"
                    )

                except Exception as e:

                    print(
                        "STM32 transmit error:",
                        e
                    )

            return

        # ====================================================
        # CRY DETECTED
        # ====================================================

        crying_message = (
            prediction
            .replace(
                "_",
                " "
            )
            .title()
        )

        status_label.config(
            text=(
                "Baby Crying Alert!\n"
                "Reason: "
                + crying_message
            ),
            font=("Times New Roman", 22),
            fg="red"
        )

        print()
        print("BABY CRY DETECTED")
        print(
            "Reason:",
            crying_message
        )

        # ====================================================
        # SEND TELEGRAM CRY ALERT
        # ====================================================

        telegram_message = (
            "🚨 Baby Crying Alert!\n"
            "Reason: "
            + crying_message
        )

        send_telegram(
            telegram_message
        )

        # ====================================================
        # SEND CRY STATUS TO STM32
        # ====================================================

        if stm is not None:

            try:

                stm.write(
                    b"CRY:ON\n"
                )

                print(
                    "Sent to STM32: CRY:ON"
                )

            except Exception as e:

                print(
                    "STM32 transmit error:",
                    e
                )

    except Exception as e:

        print()
        print("AUDIO PROCESSING ERROR")

        print(
            "ERROR TYPE:",
            type(e).__name__
        )

        print(
            "ERROR:",
            e
        )


# ============================================================
# TEST EXISTING AUDIO FILE
# ============================================================

def test_existing_audio():

    print()
    print("================================")
    print("TESTING MODEL WITH test.wav")
    print("================================")

    if os.path.isfile(
        TEST_CRY_FILE
    ):

        print(
            "test.wav found:"
        )

        print(
            TEST_CRY_FILE
        )

        audio_process(
            TEST_CRY_FILE
        )

    else:

        print(
            "ERROR: test.wav was not found."
        )

        print(
            "Expected location:"
        )

        print(
            TEST_CRY_FILE
        )


# ============================================================
# PROCESS STM32 DATA + RECORD AUDIO
# ============================================================

def process_system():

    global cry_record_counter

    # ========================================================
    # READ ALL AVAILABLE STM32 MESSAGES
    # ========================================================

    while (
        stm is not None
        and stm.in_waiting > 0
    ):

        try:

            received_message = (
                stm.readline()
                .decode(
                    "utf-8",
                    errors="ignore"
                )
                .strip()
            )

            if not received_message:

                continue

            print(
                "STM32:",
                received_message
            )

            # =================================================
            # GAS DETECTED
            # =================================================

            if received_message == (
                "ALERT:GAS DETECTED"
            ):

                status_label.config(
                    text=""
                )

                temp_label.config(
                    text=""
                )

                motion_label.config(
                    text=""
                )

                # ---------------------------------------------
                # SEND GAS TELEGRAM ALERT
                # ---------------------------------------------

                send_telegram(
                    "🚨 GAS ALERT!\n"
                    "Gas detected by Smart Nursery Guardian."
                )

                # ---------------------------------------------
                # SHOW GAS IMAGE
                # ---------------------------------------------

                if gas_alert is not None:

                    image_label.config(
                        image=gas_alert
                    )

                    image_label.image = (
                        gas_alert
                    )

            # =================================================
            # GAS CLEAR
            # =================================================

            elif received_message == (
                "STATUS:GAS CLEAR"
            ):

                image_label.config(
                    image=""
                )

                image_label.image = None

                status_label.config(
                    text="No Gas Detected",
                    font=("Times New Roman", 22),
                    fg="green"
                )

            # =================================================
            # BABY AWAKE
            # =================================================

            elif received_message == (
                "BABY AWAKE ALERT!"
            ):

                motion_label.config(
                    text="Baby Awake",
                    font=("Times New Roman", 22),
                    fg="red"
                )

            # =================================================
            # BABY SLEEP
            # =================================================

            elif received_message == (
                "Baby Returned to sleep"
            ):

                motion_label.config(
                    text="Baby SLEEP",
                    font=("Times New Roman", 22),
                    fg="green"
                )

            # =================================================
            # TEMPERATURE FORMAT 1
            # temperature:25.5
            # =================================================

            elif received_message.startswith(
                "temperature:"
            ):

                temperature_value = (
                    received_message
                    .split(
                        ":",
                        1
                    )[1]
                    .strip()
                )

                temp_label.config(
                    text=(
                        "Temperature: "
                        + temperature_value
                        + "°C"
                    )
                )

            # =================================================
            # TEMPERATURE FORMAT 2
            # temperature = 25.5°C
            # =================================================

            elif received_message.startswith(
                "temperature ="
            ):

                temperature_value = (
                    received_message
                    .split(
                        "=",
                        1
                    )[1]
                    .strip()
                )

                if temperature_value.endswith(
                    "°C"
                ):

                    display_temperature = (
                        temperature_value
                    )

                else:

                    display_temperature = (
                        temperature_value
                        + "°C"
                    )

                temp_label.config(
                    text=(
                        "Temperature: "
                        + display_temperature
                    )
                )

        except Exception as e:

            print()
            print(
                "STM32 READ ERROR"
            )

            print(
                "ERROR TYPE:",
                type(e).__name__
            )

            print(
                "ERROR:",
                e
            )

            break

    # ========================================================
    # AUDIO RECORDING TIMER
    # ========================================================

    cry_record_counter += 1

    # process_system runs every 100 ms
    # 30 x 100 ms = 3 seconds

    if cry_record_counter >= 30:

        cry_record_counter = 0

        recorded_file = record_cry(
            file_name="live_cry.wav",
            duration=3,
            sample_rate=16000
        )

        if recorded_file is not None:

            audio_process(
                recorded_file
            )

    # ========================================================
    # RUN AGAIN AFTER 100 ms
    # ========================================================

    root.after(
        100,
        process_system
    )


# ============================================================
# START PROGRAM
# ============================================================

print()
print("========================================")
print("SMART NURSERY GUARDIAN")
print("========================================")

print(
    "Python file:"
)

print(
    os.path.abspath(__file__)
)

print()
print(
    "Model path:"
)

print(
    MODEL_FILE
)

print()
print(
    "Test audio:"
)

print(
    TEST_CRY_FILE
)

print()
print(
    "STM32 port:"
)

print(
    STM_PORT
)

print()
print("========================================")
print("STARTING TEST")
print("========================================")


# ============================================================
# TEST TELEGRAM CONNECTION FIRST
# ============================================================

print()
print("========================================")
print("TESTING TELEGRAM")
print("========================================")

send_telegram(
    "✅ Smart Nursery Guardian started successfully."
)


# ============================================================
# TEST MODEL WITH test.wav FIRST
# ============================================================

test_existing_audio()


# ============================================================
# START SYSTEM LOOP
# ============================================================

root.after(
    100,
    process_system
)


# ============================================================
# START GUI
# ============================================================

root.mainloop()