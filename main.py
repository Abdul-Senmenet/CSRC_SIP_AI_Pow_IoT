import csv
import os
import time
import threading
import requests
from datetime import datetime
from collections import deque, Counter
from arduino.app_utils import *
from edge_impulse_linux.runner import ImpulseRunner

CSV_PATH = "sensor_data.csv"
MODEL_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "model.eim")

# Must match exact column order from EI training (excludes timestamp + leads_off)
FEATURE_KEYS = ["red", "ir", "ax", "ay", "az", "gx", "gy", "gz", "gsr", "ecg", "temperature"]
NUM_FEATURES = len(FEATURE_KEYS)

INFERENCE_DELAY = 0.05   # seconds between inference checks

# -------------------- Gemma (llama-server) config --------------------
GEMMA_URL = "http://192.168.1.148:8081/v1/chat/completions"
GEMMA_MAX_TOKENS = 120

# -------------------- Edge Impulse model init --------------------
runner = ImpulseRunner(MODEL_PATH)
model_info = runner.init()
INPUT_SIZE  = model_info['model_parameters']['input_features_count']
LABELS      = model_info['model_parameters']['labels']

if INPUT_SIZE % NUM_FEATURES != 0:
    raise ValueError(
        f"[EI] Mismatch: input_features_count={INPUT_SIZE} not divisible by "
        f"NUM_FEATURES={NUM_FEATURES}. Check FEATURE_KEYS against your EI training columns."
    )

WINDOW_SIZE = INPUT_SIZE // NUM_FEATURES
INFERENCE_STRIDE = max(1, WINDOW_SIZE // 2)

print(f'[EI] Model loaded | Labels: {LABELS}')
print(f'[EI] Window: {WINDOW_SIZE} samples x {NUM_FEATURES} features = {INPUT_SIZE} inputs')
print(f'[EI] Inference stride: every {INFERENCE_STRIDE} new samples')

# -------------------- Queues --------------------
csv_queue = deque()
inference_window = deque(maxlen=WINDOW_SIZE)
samples_since_inference = 0

# -------------------- 5-second aggregation state --------------------
AGGREGATE_INTERVAL = 5.0   # seconds
classification_buffer = []      # holds label entries collected during current interval
aggregate_window_start = time.time()

last_aggregate_state = None     # last finalized 5s aggregate state
disconnected_prompt_count = 0   # how many times we've nagged about disconnection
MAX_DISCONNECT_PROMPTS = 3
DISCONNECTED_LABEL = "disconnected"   # must match your EI label string exactly

llm_busy = False   # prevents overlapping LLM calls

# -------------------- CSV setup --------------------
if not os.path.exists(CSV_PATH):
    with open(CSV_PATH, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp"] + FEATURE_KEYS + ["leads_off"])


def safe_bridge_call(method, *args):
    """Wraps Bridge.call so a missing/failed MCU-side handler never crashes the app."""
    try:
        Bridge.call(method, *args)
    except Exception as e:
        print(f"[Bridge] '{method}' call failed: {e}")


def log_sample(red, ir, ax, ay, az, gx, gy, gz, gsr, ecg, leads_off, temperature):
    global samples_since_inference
    timestamp = datetime.now().isoformat()

    csv_queue.append([
        timestamp,
        red, ir,
        round(float(ax), 3), round(float(ay), 3), round(float(az), 3),
        round(float(gx), 3), round(float(gy), 3), round(float(gz), 3),
        gsr, ecg, bool(leads_off),
        round(float(temperature), 3)
    ])

    inference_window.append([
        float(red), float(ir),
        float(ax), float(ay), float(az),
        float(gx), float(gy), float(gz),
        float(gsr), float(ecg),
        float(temperature)
    ])

    samples_since_inference += 1
    return True


def query_gemma(prompt, system_prompt=None):
    """Blocking call to llama-server. Run this inside a thread."""
    messages = []
    if system_prompt:
        messages.append({"role": "system", "content": system_prompt})
    messages.append({"role": "user", "content": prompt})

    try:
        resp = requests.post(GEMMA_URL, json={
            "messages": messages,
            "max_tokens": GEMMA_MAX_TOKENS,
            "temperature": 0.7
        }, timeout=60)
        return resp.json()["choices"][0]["message"]["content"].strip()
    except Exception as e:
        print(f"[Gemma] Error: {e}")
        return None


def handle_disconnected_prompt():
    global disconnected_prompt_count, llm_busy

    if disconnected_prompt_count >= MAX_DISCONNECT_PROMPTS:
        print("[Gemma] Disconnection prompt limit reached, staying silent.")
        return

    disconnected_prompt_count += 1
    llm_busy = True

    def worker():
        global llm_busy
        prompt = (
            "The wearable health device has detected it is disconnected from the user's body. "
            "Write a brief, friendly one-sentence reminder asking the user to put on the device properly "
            "for a better experience. Keep it under 20 words."
        )
        response = query_gemma(prompt)
        if response:
            print(f"\n[Gemma -> User] {response}\n")
            safe_bridge_call("show_llm_message", response)
        llm_busy = False

    threading.Thread(target=worker, daemon=True).start()


def handle_coping_mechanism(state_label):
    global llm_busy
    llm_busy = True

    def worker():
        global llm_busy
        prompt = (
            f"The wearable health device has detected the user's physiological state is '{state_label}'. "
            f"Based on this state, give the user a short, practical coping mechanism or suggestion "
            f"(2-3 sentences max, under 35 words total). Be warm and supportive, not clinical."
        )
        response = query_gemma(prompt)
        if response:
            print(f"\n[Gemma -> User] {response}\n")
            safe_bridge_call("show_llm_message", response)
        llm_busy = False

    threading.Thread(target=worker, daemon=True).start()


def finalize_aggregate_window():
    """Called every AGGREGATE_INTERVAL seconds. Determines majority class and reacts."""
    global classification_buffer, last_aggregate_state, disconnected_prompt_count, aggregate_window_start

    if not classification_buffer:
        aggregate_window_start = time.time()
        return

    # Majority vote over the 5s window
    counts = Counter(classification_buffer)
    aggregate_state, count = counts.most_common(1)[0]
    confidence_ratio = count / len(classification_buffer)

    print(f"\n[AGGREGATE] 5s window => {aggregate_state} "
          f"({count}/{len(classification_buffer)} samples, {confidence_ratio:.0%})")

    safe_bridge_call("show_aggregate_state", aggregate_state)

    if not llm_busy:
        if aggregate_state == DISCONNECTED_LABEL:
            handle_disconnected_prompt()
        else:
            # Reset disconnect counter once reconnected
            if last_aggregate_state == DISCONNECTED_LABEL:
                disconnected_prompt_count = 0

            # Only trigger coping mechanism on a STATE CHANGE (not every 5s repeat)
            if aggregate_state != last_aggregate_state:
                handle_coping_mechanism(aggregate_state)

    last_aggregate_state = aggregate_state
    classification_buffer = []
    aggregate_window_start = time.time()


def run_inference():
    global samples_since_inference

    if len(inference_window) < WINDOW_SIZE:
        return
    if samples_since_inference < INFERENCE_STRIDE:
        return

    samples_since_inference = 0
    features = [val for sample in inference_window for val in sample]

    try:
        res = runner.classify(features)
        classification = res['result']['classification']
        top_label = max(classification, key=classification.get)
        top_confidence = classification[top_label]

        print(f'[EI] => {top_label}  ({top_confidence:.1%})')

        # Feed into 5s aggregation buffer
        classification_buffer.append(top_label)

        # Send raw per-window result to OLED
        safe_bridge_call("show_classification", top_label, float(top_confidence))

    except Exception as e:
        print(f'[EI] Inference error: {e}')


def pop_n_left(n):
    out = []
    for _ in range(n):
        if csv_queue:
            out.append(csv_queue.popleft())
    return out


def loop():
    global aggregate_window_start

    # Flush CSV buffer
    elements = pop_n_left(50)
    if elements:
        with open(CSV_PATH, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerows(elements)

    # Run classification on rolling window
    run_inference()

    # Check if 5-second aggregation interval has elapsed
    if time.time() - aggregate_window_start >= AGGREGATE_INTERVAL:
        finalize_aggregate_window()

    time.sleep(INFERENCE_DELAY)


Bridge.provide("log_sample", log_sample)
App.run(user_loop=loop)
