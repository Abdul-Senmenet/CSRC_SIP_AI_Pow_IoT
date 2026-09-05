# Senti — AI-Powered IoT Wearable for Stress & Sleep Management

> A low-cost, five-sensor wearable that classifies **seven physiological states in real time on-device** using TinyML, paired with a live web dashboard and an agentic-AI wellness companion.

<p align="center">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Arduino%20UNO%20Q-00979D">
  <img alt="TinyML" src="https://img.shields.io/badge/TinyML-Edge%20Impulse%20INT8-3B4CCA">
  <img alt="Accuracy" src="https://img.shields.io/badge/validation%20accuracy-97.6%25-brightgreen">
  <img alt="RAM" src="https://img.shields.io/badge/peak%20RAM-44%20bytes-orange">
  <img alt="Latency" src="https://img.shields.io/badge/inference-%3C1%20ms%2Fwindow-blue">
  <img alt="SDG" src="https://img.shields.io/badge/UN%20SDG-3%20Good%20Health-0A97D9">
</p>

---

## Table of Contents

- [Overview](#overview)
- [Why This Project](#why-this-project)
- [Key Results](#key-results)
- [System Architecture](#system-architecture)
- [Hardware](#hardware)
- [Firmware](#firmware)
- [Dataset](#dataset)
- [TinyML Model](#tinyml-model)
- [Senti Web Application](#senti-web-application)
- [Technology Stack](#technology-stack)
- [Repository Structure](#repository-structure)
- [Getting Started](#getting-started)
- [Limitations](#limitations)
- [Roadmap](#roadmap)
- [References](#references)
- [Team & Acknowledgements](#team--acknowledgements)
- [License](#license)

---

## Overview

Chronic stress and disrupted sleep are increasingly common among students and working professionals. Commercial wearables track basic wellness indicators, but very few combine **electrodermal (GSR), electrocardiogram (ECG) and photoplethysmography (PPG)** sensing in one low-power device, and fewer still run **machine-learning inference on the device itself** instead of streaming raw data to the cloud.

**Senti** closes that gap. It is an IoT-enabled, multi-sensor wearable built on the **Arduino UNO Q** that fuses five physiological sensors and runs an INT8-quantised neural network locally to classify the wearer's state in under a millisecond — then surfaces the result through a live web dashboard with an agentic-AI companion that suggests context-aware micro-interventions.

Developed as a **Student Innovative Project (SIP ID: 2526S0124)** funded by the Centre for Sponsored Research and Consultancy (CSRC), Anna University, Chennai.

### Classified States

| State | Description |
|---|---|
| `Normal` | Seated rest, baseline |
| `Stress` | Cognitive load / time-pressured task |
| `Sleep` | Light dozing, relaxed supine posture |
| `Stress-Sleep` | Stress-to-sleep transition (co-occurrence) |
| `Post-Workout` | Physical exertion recovery |
| `Dehydrated` | Reduced hydration state |
| `Disconnected` | Out-of-distribution / electrode-detached detector |

---

## Why This Project

Existing research systems each solve part of the problem. None solve all of it at once:

| Gap | How Senti addresses it |
|---|---|
| Single-modality sensing | **Five modalities** — PPG, ECG, GSR, IMU, temperature — on one MCU-class device |
| Limited class vocabulary | **Seven classes**, including under-explored `Stress-Sleep` and `Dehydrated` |
| Cloud-dependent inference (200–800 ms round trip, privacy risk) | **On-device INT8 inference**, `<1 ms`, no raw data leaves the device |
| Generic, non-adaptive feedback | **Agentic LLM** conditioned on the live physiological state |
| Cost — INR 15,000–80,000 for commercial units | **Sensor BOM under INR 2,000** (~2 orders of magnitude cheaper) |

---

## Key Results

| Metric | Value |
|---|---|
| Validation accuracy | **97.6%** |
| Test accuracy (after retraining) | 97.12% |
| Cross-entropy loss | 0.09 |
| AUC | 1.00 |
| Weighted precision / recall / F1 | 0.98 / 0.98 / 0.98 |
| On-device inference time | **< 1 ms per window** |
| Peak RAM (INT8 quantised) | **44 bytes** |
| Dataset size | 281,000 raw samples → 11,377 windows |
| Estimated battery life | ~7 h (3.7 V 1,200 mAh LiPo @ ~180 mA) |

Residual confusion is small and physiologically sensible: `Stress-Sleep` ↔ `Stress` (2.7%) and `Stress-Sleep` ↔ `Post-Workout` (2.7%), all of which share elevated GSR and heart rate.

---

## System Architecture

Three-tier pipeline:

```
┌──────────────────────────────────────────────────────────────┐
│  TIER 1 — DEVICE LAYER                                       │
│  Arduino UNO Q (STM32 MCU + Linux module)                    │
│  5 sensors @ 50 Hz  ──►  Edge Impulse INT8 NN  ──►  state    │
│  OLED shows live status  •  CSV over USB serial / Wi-Fi      │
└───────────────────────────┬──────────────────────────────────┘
                            │  Bridge / RouterBridge
┌───────────────────────────▼──────────────────────────────────┐
│  TIER 2 — BACKEND BRIDGE                                     │
│  Python FastAPI server                                       │
│  Parses serial stream ──► JSON REST                          │
│  Endpoints:  GET /state     GET /sensors                     │
└───────────────────────────┬──────────────────────────────────┘
                            │  polled @ 1 Hz (state) / 20 Hz (sensors)
┌───────────────────────────▼──────────────────────────────────┐
│  TIER 3 — FRONTEND (Senti web app)                           │
│  Live state card • animated ECG • 6 sensor tiles             │
│  State-history strip • daily insights • alert banner         │
│  Agentic AI chat (state-conditioned system prompt)           │
└──────────────────────────────────────────────────────────────┘
```

---

<img width="853" height="566" alt="image" src="https://github.com/user-attachments/assets/aa8a753c-081c-4e1a-af37-dacb4acafbc5" />


## Hardware

<img width="502" height="549" alt="image" src="https://github.com/user-attachments/assets/4e0d60db-3e2d-4bac-ba91-d4eb659fef1e" />

### Sensor Integration

| Module | Function | Interface | Parameters | Sampling Rate |
|---|---|---|---|---|
| **MAX30102** | PPG / HR / SpO₂ | I²C `0x57` | Red & IR raw counts | 10–100 Hz |
| **MPU6050** | IMU (accel + gyro) | I²C `0x68` | ax, ay, az, gx, gy, gz | 10–50 Hz |
| **AD8232** | ECG front-end | Analog `A0` + GPIO | ECG voltage, leads-off | ~250 Hz |
| **Grove GSR** | Galvanic skin response | Analog `A1` | Skin conductance | 10–50 Hz |
| **EMC1101** | Skin temperature | I²C `0x4C` | Temperature (°C) | 1 Hz |
| **SSD1306** | 0.96" OLED display | I²C `0x3C` | Status / classification output | On demand |

### Power Budget

| Component | Current (mA) |
|---|---|
| Arduino UNO Q | 80 |
| MAX30102 | 14 |
| MPU6050 | 3.9 |
| AD8232 | 0.17 |
| EMC1101 | 1.75 |
| SSD1306 OLED | ~10 |
| Grove GSR | ~1 |
| **Total** | **~180** |

Battery: 3.7 V LiPo, 1,200 mAh → **≈ 7 hours** continuous operation.

### Custom PCB

Designed in **EasyEDA**, approximately **120 mm × 100 mm**:

- Castellated edge footprint for the Arduino UNO Q (removable for reflashing)
- Peripheral headers for PPG, IMU, ECG, GSR and temperature modules
- 7805 linear regulator for the 5 V rail
- 100 nF decoupling capacitors on the I²C power rails
- **Isolated analog ground plane** to keep digital switching noise off the GSR and ECG traces

---

## Firmware

The firmware uses the Arduino UNO Q's **Bridge / RouterBridge** dual-processing model:

1. A **C++ sketch** on the STM32 polls all sensor channels sequentially at **50 Hz** (MAX30102 → MPU6050 → AD8232 → GSR ADC → EMC1101).
2. Each sample is serialised as a CSV line and pushed into the USB serial bridge FIFO.
3. A **Python process** (`main.py`) on the Linux module reads the bridge, parses lines into a thread-safe `deque`, and batches **50 samples** before writing to disk — minimising write latency.
4. The same process invokes the deployed Edge Impulse model via the `log_sample` bridge function for real-time classification.

### CSV Schema

```
timestamp, ppg_red, ppg_ir, ax, ay, az, gx, gy, gz, gsr_raw, ecg, leads_off, temperature
```

Timestamps are ISO 8601; uniform 20 ms spacing (50 Hz).

**Two hardware quirks worth knowing:**
- The GSR channel needs a **500 ms settling delay** to suppress capacitive coupling on the analog power rail.
- A **custom I²C driver** was required for the SSD1306 — the Adafruit GFX library does not play well with the Zephyr HAL on STM32.

---

## Dataset

The dataset is **entirely original and collected in-house**. WESAD and PhysioNet were consulted to sanity-check feature distributions and inform experiment design, but **no external benchmark data was used for training** — this keeps the model matched to this specific hardware and electrode placement.

### Participants

Eight Anna University participants, ages 18–23, recorded in a lab setting with the device on the non-dominant hand and electrodes in a **Lead I** configuration. All participants gave informed verbal consent and could withdraw at any time.

| # | Gender | Age | Status | States | Sessions |
|---|---|---|---|---|---|
| P1 | Male | 21 | UG Student | All 7 | 3 |
| P2 | Male | 20 | UG Student | All 7 | 3 |
| P3 | Female | 21 | UG Student | All 7 | 3 |
| P4 | Male | 22 | UG Student | All 7 | 2 |
| P5 | Male | 20 | Working Professional | Normal, Stress, Sleep, Dehydrated | 2 |
| P6 | Male | 23 | PG Student | All 7 | 2 |
| P7 | Female | 18 | UG Student | Normal, Stress, Post-Workout, Sleep | 2 |
| P8 | Male | 22 | Working Professional | All 7 | 3 |

### Elicitation Protocol

| State | Protocol |
|---|---|
| Normal | 10 min seated rest after a 5 min adaptation period |
| Stress | Stroop colour-word interference + timed arithmetic, 10 min |
| Sleep | Light dozing / relaxed supine in a dark room, 15 min |
| Stress-Sleep | Cognitive load task immediately followed by supine rest |
| Post-Workout | 5 min stair-climbing, then 5 min recording |
| Dehydrated | Recording after 4 h of water abstention |
| Disconnected | Deliberate sensor detachment (artefact baseline) |

Data quality was monitored live via the OLED and the Python logging window; sessions with heavy motion artefacts or poor electrode contact were repeated.

### Class Distribution

| Class | Raw Samples (50 Hz) | Duration | Windows (1000 ms / 500 ms) |
|---|---|---|---|
| Normal | 55,000 | 18 m 20 s | 1,820 |
| Stress | 72,000 | 24 m 00 s | 2,400 |
| Sleep | 48,000 | 16 m 00 s | 1,600 |
| Stress-Sleep | 28,000 | 9 m 20 s | 933 |
| Post-Workout | 32,000 | 10 m 40 s | 1,067 |
| Dehydrated | 26,000 | 8 m 40 s | 867 |
| Disconnected | 20,000 | 6 m 40 s | 667 |
| **Total** | **281,000** | **3 h 9 m 37 s** | **11,377** |

### Preprocessing

- Linear interpolation over ECG gaps during confirmed leads-off periods
- Per-session zero-mean / unit-variance normalisation of accelerometer and gyroscope channels
- Band-pass filter (0.5–4 Hz) on PPG red & IR to isolate the cardiac pulse
- Low-pass filter (5 Hz) on GSR to suppress EMG artefacts
- Exported as `cleaned_formatted_data.csv` at a uniform 20 ms sample spacing

Augmentation for robustness: Gaussian noise injection, time-series window jittering, and between-class signal interpolation, plus generatively augmented profiles covering an additional 12 student and professional personas.

---

## TinyML Model

### Impulse Configuration

| Parameter | Value |
|---|---|
| Input axes | 11 — `red, ir, ax, ay, az, gx, gy, gz, gsr, ecg, temperature` |
| Window size | 1,000 ms |
| Window stride | 500 ms |
| Frequency | 50 Hz |
| Processing block | Raw data (StandardScaler normalisation, **no DSP block**) |
| Learning block | Classification (feed-forward NN) |
| Output classes | 7 |
| Quantisation | INT8 (EON Compiler) |

### Network Architecture

```
Input (11 features)
   └─► Dense(20)  ReLU
        └─► Dense(10)  ReLU
             └─► Dense(7)  Softmax
```

Trained for **50 epochs** at a learning rate of **0.0005** on Edge Impulse GPU resources. The learned optimiser was disabled to stabilise INT8 quantisation.

### Feature Importance

Ranked by discriminative contribution:

1. **GSR** — the dominant stress indicator
2. **ax** (accelerometer X) — separates sleep, post-exercise and inactivity
3. **PPG red** — proxy for heart rate and SpO₂
4. **temperature**
5. **ay** (accelerometer Y)

ECG plays a secondary role in raw-window classification: a 1,000 ms window is too short for reliable full QRS complex detection, despite ECG's high clinical diagnostic value.

---

## Senti Web Application

A dependency-free single-page HTML5 / CSS3 / vanilla-JavaScript app that turns raw classifications into actionable wellness intelligence.

<img width="355" height="349" alt="image" src="https://github.com/user-attachments/assets/6cb704a5-5b5b-418d-932d-eeccce2ec7cb" />

<img width="482" height="383" alt="image" src="https://github.com/user-attachments/assets/ad131986-d2a2-4556-8cbb-85a6406e1812" />

<img width="1531" height="1027" alt="image" src="https://github.com/user-attachments/assets/e5b0c592-592a-407f-a871-af0be0a74b90" />


| Feature | Description |
|---|---|
| **Live State Card** | Current state with colour marker, confidence %, HR, HRV, GSR and temperature |
| **Animated ECG Waveform** | AD8232-derived trace with a scan line running at 250 Hz |
| **Six Sensor Tiles** | PPG Red, PPG IR, GSR, Ax, Ay, Az — each with a mini-graph of the last 30 readings |
| **State History Strip** | Horizontally scrollable timeline of state transitions |
| **Daily Insights Bar** | Session analysis with a 0–100 stress score |
| **Alert Banner** | Auto-appears on `Stress` / `Stress-Sleep`, links directly to the AI chat |
| **Senti Companion** | Agentic AI chat with a dynamically state-conditioned system prompt |

### Agentic AI Design

The LLM system prompt is assembled at request time from the current physiological state, confidence, and live sensor readings (HR, HRV, GSR, temperature). The model is instructed to respond with empathic, concrete micro-actions in 2–4 sentences — for example, 4-7-8 breathing for `Stress`, hydration prompts for `Dehydrated`, and wind-down techniques for `Stress-Sleep`. Contextual suggestion chips are rendered based on the active state. The conversation flow is orchestrated as a **LangGraph** state graph.

---

## Technology Stack

| Layer | Technology |
|---|---|
| Device firmware | C++ / Zephyr RTOS (Arduino UNO Q) |
| Data logging | Python 3 (`main.py` via Bridge / RouterBridge) |
| ML training | Edge Impulse Studio + EON Compiler |
| On-device inference | Edge Impulse Arduino library (INT8 NN) |
| Backend | Python FastAPI |
| Web frontend | HTML5 / CSS3 / Vanilla JS (no framework) |
| AI backend | OpenAI GPT-4o API (Claude Sonnet also supported) |
| Agent orchestration | LangGraph |
| PCB design | EasyEDA |

---

## Repository Structure

```
.
├── firmware/               # C++ sketch for the STM32 (sensor polling @ 50 Hz)
│   └── sketch.ino
├── bridge/
│   └── main.py             # Python logger — Bridge reader, batching, CSV writer
├── backend/
│   └── server.py           # FastAPI service exposing /state and /sensors
├── web/                    # Senti single-page dashboard
│   ├── index.html
│   ├── style.css
│   └── app.js
├── model/                  # Exported Edge Impulse INT8 model + Arduino library
├── data/
│   ├── raw/                # sensor_data.csv captures
│   └── cleaned_formatted_data.csv
├── hardware/               # EasyEDA schematic, PCB layout, gerbers, BOM
├── docs/                   # Report, figures, diagrams
└── README.md
```

> Adjust the tree above to match your actual layout before publishing.

---

## Getting Started

### Prerequisites

- Arduino UNO Q with the App Lab toolchain
- Python 3.10+
- An Edge Impulse account (to retrain or re-export the model)
- An OpenAI (or Anthropic) API key for the AI companion

### 1. Flash the firmware

```bash
# Open firmware/sketch.ino in Arduino App Lab, select the UNO Q board, then upload.
```

### 2. Start the data bridge and API

```bash
python -m venv .venv && source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements.txt

export OPENAI_API_KEY="sk-..."                      # Windows: set OPENAI_API_KEY=sk-...
python bridge/main.py                               # logs sensor_data.csv
uvicorn backend.server:app --reload --port 8000     # serves /state and /sensors
```

### 3. Open the dashboard

```bash
cd web && python -m http.server 5500
# then visit http://localhost:5500
```

### 4. (Optional) Retrain the model

Upload `data/cleaned_formatted_data.csv` to Edge Impulse Studio, create an impulse with a 1,000 ms window / 500 ms stride raw-data block over 11 axes, attach the classifier described above, train, then export as an **Arduino library with INT8 (EON Compiler)** quantisation.

> Never commit API keys. Keep them in a local `.env` file and add `.env` to `.gitignore`.

---

## Limitations

This is a research prototype, and the scope was deliberately bounded:

- **Not a certified medical device.** No regulatory clearance, and no substitute for clinical diagnosis.
- **No extensive clinical validation** against gold standards such as polysomnography or cortisol assays.
- **Small cohort** — eight participants, ages 18–23, all from a single institution. Generalisation across broader demographics is untested.
- **USB tether** still required for data logging in the current prototype, despite the ~7 h battery estimate.
- **Development-board form factor**, not yet a wearable enclosure.
- Short classification windows limit ECG's contribution; HRV features are not yet computed on-device.

---

## Roadmap

- [ ] **PCB miniaturisation** — smaller board, 3D-printed wristband enclosure, LiPo battery management
- [ ] **Expanded dataset** — 50+ participants across age groups and professions, validated against the Perceived Stress Scale and Pittsburgh Sleep Quality Index
- [ ] **HRV & sleep staging** — RMSSD and LF/HF in firmware; rule-based Light/Deep/REM staging from IMU + PPG
- [ ] **Mobile app** — React Native for iOS and Android with push alerts and daily sleep summaries
- [ ] **Longitudinal personalisation** — vector database for cross-session memory and adaptive recommendations
- [ ] **Clinical validation** — partnership for PSG/cortisol comparison; IS 13450 / IEC 60601 compliance review
- [ ] **IP & commercialisation** — provisional patent; incubation via AIC Anna Incubator and iTNT Hub

---

## References

1. M. Rostami, "LSTM-based real-time stress detection using PPG signals on Raspberry Pi," *IET Wireless Sensor Systems*, 2024.
2. P. Srivastava, N. Shah, and K. Jaiswal, "Microcontroller-Based EdgeML: Health Monitoring for Stress and Sleep via HRV," *Proc. 1st Int. Conf. on AI Sensors*, Eng. Proc., vol. 78, no. 1, p. 3, Dec. 2024.
3. A. A. Al-Atawi et al., "Stress Monitoring Using Machine Learning, IoT and Wearable Sensors," *Sensors*, vol. 23, no. 21, Art. 8875, Oct. 2023.
4. A. Abd-alrazaq et al., "The Performance of Wearable AI in Detecting Stress Among Students: Systematic Review and Meta-Analysis," *J. Med. Internet Res.*, vol. 26, Art. e52622, Jan. 2024.
5. A. Pinge et al., "Detection and Monitoring of Stress Using Wearables: A Systematic Review," *Frontiers in Computer Science*, vol. 6, Art. 1478851, Dec. 2024.
6. J. L. Abellán et al., "An edge-stream computing infrastructure for real-time analysis of wearable sensors data," *Future Generation Computer Systems*, vol. 93, pp. 515–528, Apr. 2019.
7. P. Warden and D. Situnayake, *TinyML: Machine Learning with TensorFlow Lite on Arduino and Ultra-Low-Power Microcontrollers*. O'Reilly Media, 2020.
8. R. Castaldo et al., "Ultra-short term HRV features as surrogates of short term HRV," *BMC Medical Informatics and Decision Making*, vol. 19, Art. 12, 2019.
9. M. Z. Poh, N. C. Swenson, and R. W. Picard, "A wearable sensor for unobtrusive, long-term assessment of electrodermal activity," *IEEE Trans. Biomed. Eng.*, vol. 57, no. 5, pp. 1243–1252, May 2010.
10. Edge Impulse Inc., "Edge Impulse Studio Documentation." https://docs.edgeimpulse.com

---

## Team & Acknowledgements

**Developed by**
- S. Abdullah
- A. Alston Samuel Peter
- A. Mohammed Arshad

**Guided by**
- Dr. K. Manimala — Associate Professor, Dept. of Information Science and Technology
- Dr. S. Sangeetha — Assistant Professor, Dept. of Electronics and Communication Engineering

Department of Information Science and Technology
College of Engineering Guindy (CEG), Anna University, Chennai — 600 025

**Funded by** the Centre for Sponsored Research and Consultancy (CSRC), Anna University, under the Student Innovative Project scheme 2025–2026 (**SIP ID: 2526S0124**).

With thanks to Dr. S. Swamynathan (former HoD) and Dr. P. Yogesh (HoD) for their support, the AU-IST office for administrative assistance, the eight student volunteers who contributed recordings, and the Edge Impulse, Arduino and Zephyr RTOS communities.

---

## License

No license has been selected yet. Add one (for example, MIT for the software and CERN-OHL-S for the hardware) before making the repository public, or all rights remain reserved by default.

---

<p align="center"><em>Contributing to UN Sustainable Development Goal 3 — Good Health and Well-Being.</em></p>
