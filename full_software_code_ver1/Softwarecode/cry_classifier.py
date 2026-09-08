import numpy as np
import pandas as pd
import librosa
import webrtcvad
import noisereduce


class CryClassifier:
    def __init__(
        self,
        model,
        feature_columns,
        background_noise,
        vad_aggressiveness=3,
        threshold=0.05,
    ):
        self.model = model
        self.feature_columns = feature_columns
        self.background_noise = background_noise
        self.vad_aggressiveness = vad_aggressiveness
        self.threshold = threshold

    def _check_speech_fraction(self, wave_form_new, sample_rate, frame_size=320):
        vad = webrtcvad.Vad(self.vad_aggressiveness)
        num_frames = len(wave_form_new) // frame_size
        speech_count = 0
        for f in range(num_frames):
            start = f * frame_size
            chunk = wave_form_new[start : start + frame_size]
            if vad.is_speech(chunk.tobytes(), sample_rate):
                speech_count += 1
        return speech_count / num_frames if num_frames > 0 else 0

    def predict(self, file_path):
        wave_form, sample_rate = librosa.load(file_path, sr=16000)
        wave_form_new = wave_form * (2**15)
        wave_form_new = np.clip(wave_form_new, -32768, 32767).astype(np.int16)

        speech_fraction = self._check_speech_fraction(wave_form_new, sample_rate)
        if speech_fraction <= self.threshold:
            return "no cry detected"

        cleaned_wave = noisereduce.reduce_noise(
            y=wave_form, sr=sample_rate, y_noise=self.background_noise, stationary=True
        )

        mfcc = librosa.feature.mfcc(y=cleaned_wave, sr=16000)
        mfcc_mean = np.mean(mfcc, axis=1)
        mfcc_std = np.std(mfcc, axis=1)
        duration = len(cleaned_wave) / 16000
        rms_value = np.mean(librosa.feature.rms(y=cleaned_wave))
        zcr_value = np.mean(librosa.feature.zero_crossing_rate(cleaned_wave))
        f0, _, _ = librosa.pyin(cleaned_wave, fmin=50, fmax=500)
        f0 = np.nan_to_num(f0).mean()

        row = {"duration": duration, "rms": rms_value, "f0": f0, "zcr": zcr_value}
        for idx, value in enumerate(mfcc_mean):
            row[f"mfcc_{idx}_mean"] = value
        for idx, value in enumerate(mfcc_std):
            row[f"mfcc_{idx}_std"] = value

        row_df = pd.DataFrame([row])[self.feature_columns]
        return self.model.predict(row_df)[0]
