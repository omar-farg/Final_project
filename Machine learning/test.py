import os
import joblib
from cry_classifier import CryClassifier

script_dir = os.path.dirname(os.path.abspath(__file__))
pkl_path = os.path.join(script_dir, "cry_classifier.pkl")

classifier = joblib.load(pkl_path)
result = classifier.predict(
    "/data/me/New folder/mind cloud/mind2026/Final project/Machine Learning/donateacry_corpus/burping/AEA8AE04-D00E-48A7-8A0B-6D87E2175121-1430563241-1.0-f-72-bu.wav"
)
print(result)
