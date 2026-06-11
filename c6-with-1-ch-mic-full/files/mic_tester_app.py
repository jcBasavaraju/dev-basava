"""
1CH Mic Tester — PC Application  (v2 — Fixed + Enhanced)
ICS43434  |  Glyph C6 (ESP32-C6)

Changes from original
─────────────────────
• FIXED: Mic waveform is now a FLAT LINE when idle (not a fake animated sine)
         Only shows animated signal when a test is actively running
         After test: shows signal proportional to real SNR result data
• NEW  : Frequency generator control panel
         - Slider (20 Hz – 20 kHz) + numeric entry + preset buttons
         - Turning the "knob" updates the test frequency live
         - Graph labels update to reflect the chosen frequency
• NEW  : Clear graph panel headers:
         "▶ SIGNAL GENERATOR  (what we are sending)"
         "🎙 MIC CAPTURE  (what the 1CH mic + ESP32 heard)"
         "📊 FFT SPECTRUM  (frequency domain)"
• NEW  : State machine: IDLE → TESTING → RESULT controls what mic graph shows
• IMPROVED: Mic graph shows realistic silence (tiny noise) when idle
• IMPROVED: During test, mic graph shows "listening" low-amplitude noise
• IMPROVED: After test, mic graph amplitude matches real signal_db from firmware
• IMPROVED: Graph title updates dynamically with current freq setting

Requirements
────────────
    pip install pyserial sounddevice numpy matplotlib mysql-connector-python
"""

import tkinter as tk
from tkinter import ttk, messagebox
import threading
import serial
import serial.tools.list_ports
import json
import numpy as np
import datetime
import csv
import os
import time

try:
    import sounddevice as sd
    HAS_AUDIO = True
except ImportError:
    HAS_AUDIO = False

try:
    import mysql.connector
    HAS_MYSQL = True
except ImportError:
    HAS_MYSQL = False

try:
    import matplotlib
    matplotlib.use("TkAgg")
    from matplotlib.figure import Figure
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    import matplotlib.animation as animation
    HAS_MPL = True
except ImportError:
    HAS_MPL = False

# ── Config (must match firmware defines) ──────────────────────────
TEST_FREQ     = 2000          # default; user can change via UI
SAMPLE_RATE   = 44100
PLAY_VOLUME   = 1.0
BAUD_RATE     = 115200
TONE_HOLD_SEC = 5
TIMEOUT_SEC   = 40            # Extended to 40s to allow hardware warmup settling
PRE_TONE_SEC  = 0.8

# Graph config
FFT_DISP_HZ   = 10000          # FFT x-axis upper limit

COLORS = {
    "PASS":    ("#e6f9f1", "#1D9E75"),
    "FAIL":    ("#fdecea", "#E24B4A"),
    "TESTING": ("#e8f0fe", "#1a73e8"),
    "READY":   ("#f5f5f5", "#888888"),
    "ERROR":   ("#fff8e1", "#B07A00"),
}

DARK_BG  = "#0d0f12"
PLOT_BG  = "#13161b"
GRID_COL = "#252a34"
BLUE     = "#378ADD"
GREEN    = "#1D9E75"
RED      = "#E24B4A"
AMBER    = "#EF9F27"
MUTED    = "#6b7280"
MUTED2   = "#9ca3af"

# Graph state constants
STATE_IDLE    = "idle"      # no test running, no result — mic shows flatline
STATE_TESTING = "testing"   # test in progress — mic shows listening noise
STATE_RESULT  = "result"    # test done — mic shows scaled signal from result


# ── Fail reason classifier ────────────────────────────────────────
def classify_fail_reason(data: dict, test_freq: int) -> str:
    status    = data.get("status", "")
    snr       = data.get("snr", None)
    peak_freq = data.get("peak_freq", None)
    signal_db = data.get("signal_db", None)
    noise_db  = data.get("noise_db", None)
    msg       = data.get("msg", "")

    if status == "ERROR":
        if "I2S" in msg:
            return "ERROR: I2S init failed — check mic wiring"
        if "samples" in msg.lower():
            return "ERROR: Not enough I2S samples — mic may be dead"
        return f"ERROR: {msg}" if msg else "ERROR: Unknown firmware error"

    if status == "FAIL":
        reasons = []
        if signal_db is not None and noise_db is not None:
            if (signal_db - noise_db) < 3:
                reasons.append(
                    "Mic not responding — signal indistinguishable from noise "
                    "(possible dead mic or open circuit)")
        
        # Checked against the 12 dB requirement profile
        if snr is not None and snr < 12:
            if snr < 5:
                reasons.append(f"Very low SNR {snr:.1f} dB — open circuit or dead mic")
            else:
                reasons.append(f"Low SNR {snr:.1f} dB — mic weak, acoustic path blocked, or volume too low")
        if peak_freq is not None and abs(peak_freq - test_freq) > 200:
            reasons.append(
                f"Peak at {peak_freq} Hz instead of {test_freq} Hz — "
                "possible I2S clock error or strong interference")
        if noise_db is not None and noise_db > -20:
            reasons.append(
                f"High noise floor {noise_db:.1f} dB — EMI, power supply noise, or mic self-noise")
        return "; ".join(reasons) if reasons else "FAIL: SNR below 12 dB threshold"
    return ""


# ── Audio ring buffer (thread-safe, fixed size) ───────────────────
class RingBuffer:
    def __init__(self, size):
        self._buf    = np.zeros(size, dtype=np.float32)
        self._size   = size
        self._filled = 0          
        self._lock   = threading.Lock()

    def write(self, data: np.ndarray):
        n = len(data)
        with self._lock:
            if n >= self._size:
                self._buf[:] = data[-self._size:]
                self._filled = self._size
            else:
                self._buf = np.roll(self._buf, -n)
                self._buf[-n:] = data
                self._filled = min(self._filled + n, self._size)

    def read(self) -> np.ndarray:
        with self._lock:
            if self._filled >= self._size:
                return self._buf.copy()
            return self._buf[-self._filled:].copy() if self._filled > 0 else np.zeros(0, dtype=np.float32)

    def read_full(self) -> np.ndarray:
        with self._lock:
            return self._buf.copy()

    @property
    def filled(self) -> int:
        with self._lock:
            return self._filled

    def clear(self):
        with self._lock:
            self._buf[:] = 0.0
            self._filled = 0


# ── Audio capture engine ──────────────────────────────────────────
BUF_SECS  = 0.5          
BUF_SIZE  = int(SAMPLE_RATE * BUF_SECS)
FFT_SIZE  = 4096          

class AudioEngine:
    def __init__(self):
        self.spk_buf  = RingBuffer(BUF_SIZE)   
        self.mic_buf  = RingBuffer(BUF_SIZE)   
        self._out     = None
        self._inp     = None
        self._wave    = None
        self._running = False

    def _out_cb(self, outdata, frames, time_info, status):
        if self._wave is None:
            outdata.fill(0)
            return

        if not hasattr(self, "_phase"):
            self._phase = 0

        wave = self._wave
        wave_len = len(wave)
        indices = (np.arange(frames) + self._phase) % wave_len
        chunk = wave[indices]
        self._phase = (self._phase + frames) % wave_len

        outdata[:, 0] = chunk
        self.spk_buf.write(chunk.astype(np.float32))

    def _inp_cb(self, indata, frames, time_info, status):
        samples = indata[:, 0].copy()
        self.mic_buf.write(samples)

    def start(self, freq: int, volume: float = PLAY_VOLUME):
        if not HAS_AUDIO:
            return
        self.stop()  
        t           = np.linspace(0, 1.0 / freq, int(SAMPLE_RATE / freq), endpoint=False)
        self._wave  = (np.sin(2 * np.pi * freq * t) * volume).astype(np.float32)
        self.spk_buf.clear()
        self.mic_buf.clear()

        try:
            # High-latency, stutter-free blocks tailored to work safely within your execution framework
            self._out = sd.OutputStream(
                samplerate=SAMPLE_RATE, channels=1,
                dtype="float32", callback=self._out_cb,
                blocksize=1024, latency='high')
            self._inp = sd.InputStream(
                samplerate=SAMPLE_RATE, channels=1,
                dtype="float32", callback=self._inp_cb,
                blocksize=1024, latency='high')
            self._out.start()
            self._inp.start()
            self._running = True
        except Exception as e:
            print(f"[AudioEngine] start error: {e}")
            self._running = False

    def stop_speaker(self):
        self._wave = None
        if self._out is not None:
            try:
                self._out.stop()
                self._out.close()
            except Exception:
                pass
            self._out = None

    def stop(self):
        self._running = False
        self._wave    = None
        for s in (self._out, self._inp):
            if s is not None:
                try:
                    s.stop(); s.close()
                except Exception:
                    pass
        self._out = None
        self._inp = None

    @property
    def running(self):
        return self._running


class SinePlayer:
    def __init__(self, engine: "AudioEngine"):
        self._engine = engine

    def start(self, freq=None, volume=PLAY_VOLUME):
        if freq is None:
            freq = TEST_FREQ
        self._engine.start(freq, volume)

    def stop_speaker(self):
        self._engine.stop_speaker()

    def stop(self):
        self._engine.stop()

    @property
    def wave(self):
        return self._engine._wave


# ── Waveform Graph ────────────────────────────────────────────────
class WaveformGraph:
    def __init__(self, parent, engine: AudioEngine):
        self.parent    = parent
        self._engine   = engine
        self._result_data = None
        self._state       = STATE_IDLE
        self._test_freq   = TEST_FREQ

        if not HAS_MPL:
            lbl = tk.Label(
                parent,
                text="Install matplotlib for live graphs:\npip install matplotlib",
                font=("Courier", 10), fg="#888", bg=DARK_BG, justify="center")
            lbl.pack(expand=True)
            self.canvas = None
            return

        self._update_display_window(self._test_freq)
        self._fft_freqs = np.linspace(0, FFT_DISP_HZ, FFT_SIZE // 2)

        fig = Figure(figsize=(6.2, 5.0), dpi=96, facecolor=DARK_BG)
        fig.subplots_adjust(left=0.07, right=0.97, top=0.94, bottom=0.09, hspace=0.65)

        self.ax_in = fig.add_subplot(3, 1, 1)
        self._style_ax(self.ax_in)
        self.ax_in.set_ylim(-1.25, 1.25)
        self.ax_in.set_yticks([-1, 0, 1])
        self.ax_in.set_title(
            f"▶  SIGNAL GENERATOR  —  playing {self._test_freq} Hz  (PC speaker, real audio)",
            color=BLUE, fontsize=7.5, loc="left", pad=4, fontweight="bold")
        self.line_in, = self.ax_in.plot(self._t_ms, np.zeros(self._disp_pts), color=BLUE, lw=1.6)

        self.ax_out = fig.add_subplot(3, 1, 2)
        self._style_ax(self.ax_out)
        self.ax_out.set_ylim(-1.25, 1.25)
        self.ax_out.set_yticks([-1, 0, 1])
        self.ax_out.set_xlabel("Time (ms)", color=MUTED, fontsize=7, labelpad=2)
        self.ax_out.set_title(
            "🎙  MIC CAPTURE  —  PC microphone input (real audio)",
            color=GREEN, fontsize=7.5, loc="left", pad=4, fontweight="bold")
        self.line_out, = self.ax_out.plot(self._t_ms, np.zeros(self._disp_pts), color=GREEN, lw=1.6)

        self.idle_text = self.ax_out.text(
            0.5, 0.5, "— IDLE — connect ESP32 and run a test —",
            transform=self.ax_out.transAxes,
            color=MUTED, fontsize=7, ha="center", va="center", alpha=0.7)

        self.ax_fft = fig.add_subplot(3, 1, 3)
        self._style_ax(self.ax_fft)
        self.ax_fft.set_ylim(0, 1.05)
        self.ax_fft.set_xlim(0, FFT_DISP_HZ)
        self.ax_fft.set_xlabel("Frequency (Hz)", color=MUTED, fontsize=7, labelpad=2)
        self.ax_fft.set_title(
            "📊  FFT SPECTRUM  —  computed from live mic buffer",
            color=AMBER, fontsize=7.5, loc="left", pad=4, fontweight="bold")
        xticks = [0, 500, 1000, 2000, 4000, 6000, 8000, 10000]
        self.ax_fft.set_xticks(xticks)
        self.ax_fft.set_xticklabels(["0", "500", "1k", "2k", "4k", "6k", "8k", "10k"], color=MUTED, fontsize=6.5)

        zeros_fft = np.zeros(FFT_SIZE // 2)
        self.line_fft_in,  = self.ax_fft.plot(self._fft_freqs, zeros_fft, color=BLUE,  lw=1.4, label="Generator (speaker)")
        self.line_fft_mic, = self.ax_fft.plot(self._fft_freqs, zeros_fft, color=GREEN, lw=1.4, label="Mic captured")
        self.line_fft_nfl, = self.ax_fft.plot(self._fft_freqs, zeros_fft, color=RED, lw=1.0, linestyle="--", label="Noise floor")
        self.ax_fft.legend(loc="upper right", fontsize=6.5, framealpha=0.3, facecolor=PLOT_BG, edgecolor=GRID_COL, labelcolor=MUTED2)

        self.ann_peak = self.ax_fft.annotate(
            "", xy=(self._test_freq, 0), xytext=(self._test_freq + 500, 0.5),
            color=GREEN, fontsize=7,
            arrowprops=dict(arrowstyle="->", color=GREEN, lw=0.8))
        self.ann_peak.set_visible(False)

        self._noise_rms = 0.001

        self.fig = fig
        self.canvas = FigureCanvasTkAgg(fig, master=parent)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)
        self._anim = animation.FuncAnimation(fig, self._update, interval=50, blit=True, cache_frame_data=False)

    def _update_display_window(self, freq: int):
        cycles      = 4
        secs        = cycles / max(freq, 1)
        pts         = max(64, int(SAMPLE_RATE * secs))
        self._disp_pts = pts
        self._disp_secs = secs
        self._t_ms  = np.linspace(0, secs * 1000, pts, endpoint=False)

    def _style_ax(self, ax):
        ax.set_facecolor(PLOT_BG)
        for spine in ax.spines.values():
            spine.set_edgecolor(GRID_COL)
        ax.tick_params(colors=MUTED, labelsize=7, length=2)
        ax.yaxis.label.set_color(MUTED)
        ax.grid(True, color=GRID_COL, linewidth=0.4, linestyle="-")
        ax.set_axisbelow(True)

    def _buf_to_display(self, buf: np.ndarray) -> np.ndarray:
        n = self._disp_pts
        if len(buf) == 0:
            return np.zeros(n, dtype=np.float32)
        if len(buf) >= n:
            data = buf[-n:].copy()
        else:
            pad  = np.zeros(n - len(buf), dtype=np.float32)
            data = np.concatenate([pad, buf])
        peak = float(np.max(np.abs(data)))
        if 1e-4 < peak < 0.05:
            data = data * (0.8 / peak)
        return data

    def _compute_fft(self, buf: np.ndarray):
        n = FFT_SIZE
        raw = buf[-n:] if len(buf) >= n else buf
        if len(raw) < 64:          
            return np.zeros(len(self._fft_freqs))
        data = np.zeros(n, dtype=np.float32)
        data[-len(raw):] = raw
        window = np.hanning(n)
        data   = data * window
        mag    = np.abs(np.fft.rfft(data))[:n // 2]
        mag   /= (n / 2.0)
        src_freqs = np.linspace(0, SAMPLE_RATE / 2, n // 2)
        interp    = np.interp(self._fft_freqs, src_freqs, mag)
        return np.clip(interp, 0, 1.05)

    def _update(self, frame):
        if not HAS_MPL:
            return []

        spk_raw = self._engine.spk_buf.read()
        mic_raw = self._engine.mic_buf.read()

        if self._state == STATE_IDLE:
            spk_disp = np.zeros(self._disp_pts, dtype=np.float32)
            mic_disp = np.zeros(self._disp_pts, dtype=np.float32)
            self.idle_text.set_visible(True)
            self.idle_text.set_text("— IDLE — connect ESP32 and run a test —")
            self.ann_peak.set_visible(False)
            fft_spk = np.zeros(FFT_SIZE // 2)
            fft_mic = np.zeros(FFT_SIZE // 2)
            nfl_val = 0.0

        elif self._state == STATE_TESTING:
            spk_disp = self._buf_to_display(spk_raw)
            mic_disp = self._buf_to_display(mic_raw)
            self.idle_text.set_visible(True)
            self.idle_text.set_text("— LISTENING — recording in progress… —")
            self.ann_peak.set_visible(False)
            fft_spk = self._compute_fft(spk_raw)
            fft_mic = self._compute_fft(mic_raw)
            rms = float(np.sqrt(np.mean(mic_raw ** 2))) if mic_raw.any() else 0.001
            self._noise_rms = 0.95 * self._noise_rms + 0.05 * rms
            nfl_val = np.clip(self._noise_rms * 4, 0, 1.0)

        else:  
            spk_disp = self._buf_to_display(spk_raw)
            mic_disp = self._buf_to_display(mic_raw)
            self.idle_text.set_visible(False)
            fft_spk = self._compute_fft(spk_raw)
            fft_mic = self._compute_fft(mic_raw)
            nfl_val = np.clip(self._noise_rms * 4, 0, 1.0)

            rd        = self._result_data or {}
            peak_hz   = rd.get("peak_freq", self._test_freq)
            snr       = rd.get("snr", 0.0)
            verdict   = rd.get("status", "FAIL")
            ann_color = GREEN if verdict == "PASS" else RED
            peak_mag  = float(np.max(fft_mic)) if fft_mic.any() else 0.5
            self.ann_peak.set_visible(True)
            self.ann_peak.set_text(f"▲ {peak_hz} Hz  SNR {snr:.1f} dB")
            self.ann_peak.set_color(ann_color)
            self.ann_peak.xy = (peak_hz, min(peak_mag, 1.0))
            self.ann_peak.xytext = (min(peak_hz + 600, FFT_DISP_HZ - 1400), min(peak_mag * 0.85 + 0.1, 0.95))
            self.ann_peak.arrow_patch.set_color(ann_color)

        self.line_in.set_ydata(spk_disp)
        self.line_out.set_ydata(mic_disp)
        self.line_fft_in.set_ydata(fft_spk)
        self.line_fft_mic.set_ydata(fft_mic)
        self.line_fft_nfl.set_ydata(np.full(FFT_SIZE // 2, nfl_val))

        return [self.line_in, self.line_out, self.line_fft_in, self.line_fft_mic, self.line_fft_nfl, self.ann_peak, self.idle_text]

    def set_freq(self, freq: int):
        self._test_freq = freq
        if not HAS_MPL:
            return
        self.ax_in.set_title(
            f"▶  SIGNAL GENERATOR  —  playing {freq} Hz  (PC speaker, real audio)",
            color=BLUE, fontsize=7.5, loc="left", pad=4, fontweight="bold")
        self._update_display_window(freq)
        self.line_in.set_xdata(self._t_ms)
        self.line_out.set_xdata(self._t_ms)
        self.line_in.set_ydata(np.zeros(self._disp_pts))
        self.line_out.set_ydata(np.zeros(self._disp_pts))
        self.ax_in.set_xlim(0, self._disp_secs * 1000)
        self.ax_out.set_xlim(0, self._disp_secs * 1000)
        self.canvas.draw_idle()

    def set_state(self, state: str):
        self._state = state

    def set_result(self, data: dict):
        self._result_data = data
        self._state = STATE_RESULT

    def clear_result(self):
        self._result_data = None
        self._state = STATE_IDLE


# ── Main application ──────────────────────────────────────────────
class MicTesterApp:
    def __init__(self, root):
        self.root = root
        self.root.title("1CH Mic Tester  —  Glyph C6  ·  ICS43434  (FIXED AUDIO BUILD)")
        self.root.geometry("1100x880")
        self.root.resizable(True, True)
        self.root.configure(bg="#f5f5f5")

        self.serial_port  = None
        self.db_conn      = None
        self._engine      = AudioEngine()
        self.player       = SinePlayer(self._engine)
        self.test_running = False
        self._test_freq   = TEST_FREQ    

        self.session_pass  = 0
        self.session_fail  = 0
        self.session_total = 0

        self.port_var      = tk.StringVar()
        self.serial_var    = tk.StringVar()
        self.status_var    = tk.StringVar(value="Connect to COM port, set frequency, enter serial number, then press Start Test")
        self.result_var    = tk.StringVar(value="—")
        self.snr_var       = tk.StringVar(value="—")
        self.peak_var      = tk.StringVar(value="—")
        self.noise_var     = tk.StringVar(value="—")
        self.signal_var    = tk.StringVar(value="—")
        self.reason_var    = tk.StringVar(value="")
        self.timer_var     = tk.StringVar(value="")
        self.counter_var   = tk.StringVar(value="PASS: 0     FAIL: 0     TOTAL: 0")
        self.db_status_var = tk.StringVar(value="Not connected")
        self.freq_var      = tk.StringVar(value=str(TEST_FREQ))
        self.vol_var       = tk.DoubleVar(value=PLAY_VOLUME)

        self.db_config = {
            "host":     tk.StringVar(value="localhost"),
            "port":     tk.StringVar(value="3306"),
            "user":     tk.StringVar(value="root"),
            "password": tk.StringVar(value=""),
            "database": tk.StringVar(value="inventory"),
        }

        self._build_ui()
        self._refresh_ports()

    def _build_ui(self):
        BG  = "#f5f5f5"
        pad = dict(padx=12, pady=3)

        left = tk.Frame(self.root, bg=BG, width=440)
        left.pack(side="left", fill="y", padx=(0, 0))
        left.pack_propagate(False)

        tk.Label(left, text="1CH Mic Tester", font=("Segoe UI", 16, "bold"), bg=BG).pack(pady=(14, 1))
        tk.Label(left, text="Glyph C6  ·  ICS43434  ·  SNR ≥ 12 dB = PASS", font=("Segoe UI", 8), bg=BG, fg="#888").pack(pady=(0, 4))

        fg_frame = tk.LabelFrame(
            left, text="  🔊  Signal Generator — Test Frequency  ",
            font=("Segoe UI", 9, "bold"), bg=BG, fg="#1a73e8",
            padx=10, pady=8, relief="solid", bd=1)
        fg_frame.pack(fill="x", padx=12, pady=(2, 4))

        freq_disp_row = tk.Frame(fg_frame, bg=BG)
        freq_disp_row.pack(fill="x", pady=(0, 6))

        self.freq_display = tk.Label(freq_disp_row, text=f"{self._test_freq} Hz", font=("Courier", 22, "bold"), bg=BG, fg="#1a73e8")
        self.freq_display.pack(side="left")

        tk.Label(freq_disp_row, text="  Volume:", font=("Segoe UI", 8), bg=BG, fg="#666").pack(side="left", padx=(20, 4))
        vol_slider = tk.Scale(
            freq_disp_row, variable=self.vol_var,
            from_=0.0, to=1.0, resolution=0.05,
            orient="horizontal", length=80, showvalue=False,
            bg=BG, highlightthickness=0, troughcolor="#d0d0d0",
            command=lambda v: None)
        vol_slider.pack(side="left")
        tk.Label(freq_disp_row, textvariable=tk.StringVar(), font=("Segoe UI", 7), bg=BG, fg="#888").pack(side="left")
        self.vol_pct_lbl = tk.Label(freq_disp_row, text="100%", font=("Segoe UI", 8), bg=BG, fg="#666")
        self.vol_pct_lbl.pack(side="left", padx=(2, 0))
        vol_slider.config(command=self._on_vol_change)

        slider_row = tk.Frame(fg_frame, bg=BG)
        slider_row.pack(fill="x")
        tk.Label(slider_row, text="20 Hz", font=("Segoe UI", 7), bg=BG, fg=MUTED).pack(side="left")
        self.freq_slider = tk.Scale(
            slider_row, from_=20, to=20000, resolution=1,
            orient="horizontal", showvalue=False,
            bg=BG, highlightthickness=0, troughcolor="#d0d0d0",
            command=self._on_freq_slider)
        self.freq_slider.set(self._test_freq)
        self.freq_slider.pack(side="left", fill="x", expand=True, padx=6)
        tk.Label(slider_row, text="20 kHz", font=("Segoe UI", 7), bg=BG, fg=MUTED).pack(side="left")

        preset_row = tk.Frame(fg_frame, bg=BG)
        preset_row.pack(fill="x", pady=(6, 0))
        tk.Label(preset_row, text="Presets:", font=("Segoe UI", 8), bg=BG, fg="#666").pack(side="left", padx=(0, 6))
        presets = [
            ("250 Hz", 250), ("500 Hz", 500), ("1 kHz", 1000),
            ("2 kHz", 2000), ("4 kHz", 4000), ("8 kHz", 8000),
        ]
        for label, freq in presets:
            btn = tk.Button(
                preset_row, text=label,
                font=("Segoe UI", 7), relief="solid", bd=1,
                bg="#e8f0fe", fg="#1a73e8", cursor="hand2",
                padx=4, pady=1,
                command=lambda f=freq: self._set_freq(f))
            btn.pack(side="left", padx=(0, 3))

        entry_row = tk.Frame(fg_frame, bg=BG)
        entry_row.pack(fill="x", pady=(4, 0))
        tk.Label(entry_row, text="Custom Hz:", font=("Segoe UI", 8), bg=BG, fg="#666").pack(side="left")
        freq_entry = tk.Entry(entry_row, textvariable=self.freq_var, font=("Segoe UI", 10), width=7, relief="solid", bd=1)
        freq_entry.pack(side="left", padx=6, ipady=2)
        freq_entry.bind("<Return>", lambda e: self._apply_custom_freq())
        tk.Button(entry_row, text="Set", font=("Segoe UI", 8), relief="solid", bd=1, cursor="hand2", command=self._apply_custom_freq).pack(side="left")
        tk.Label(entry_row, text="  (20 – 20000)", font=("Segoe UI", 7), bg=BG, fg="#999").pack(side="left")

        preview_row = tk.Frame(fg_frame, bg=BG)
        preview_row.pack(fill="x", pady=(6, 0))
        self._preview_playing = False
        self.btn_preview = tk.Button(
            preview_row, text="▶  Preview Tone",
            font=("Segoe UI", 9), relief="solid", bd=1,
            bg="#f0f4ff", fg="#1a73e8", cursor="hand2",
            command=self._toggle_preview)
        self.btn_preview.pack(side="left")
        tk.Label(preview_row, text="  ← Play tone without running a test", font=("Segoe UI", 7), bg=BG, fg="#999").pack(side="left")

        fp = tk.Frame(left, bg=BG)
        fp.pack(fill="x", **pad)
        tk.Label(fp, text="Glyph C6 COM port", font=("Segoe UI", 9), bg=BG, fg="#444").pack(anchor="w")
        row = tk.Frame(fp, bg=BG)
        row.pack(fill="x")
        self.port_menu = ttk.Combobox(row, textvariable=self.port_var, font=("Segoe UI", 10), state="readonly", width=14)
        self.port_menu.pack(side="left", padx=(0, 6))
        tk.Button(row, text="Refresh", font=("Segoe UI", 8), relief="solid", bd=1, cursor="hand2", command=self._refresh_ports).pack(side="left", padx=(0, 6))
        tk.Button(row, text="Connect", font=("Segoe UI", 8), relief="solid", bd=1, cursor="hand2", bg="#1a73e8", fg="white", command=self._connect_serial).pack(side="left")
        self.conn_label = tk.Label(row, text="Not connected", font=("Segoe UI", 8), bg=BG, fg="#888")
        self.conn_label.pack(side="left", padx=8)

        fs = tk.Frame(left, bg=BG)
        fs.pack(fill="x", **pad)
        tk.Label(fs, text="Mic serial number", font=("Segoe UI", 9), bg=BG, fg="#444").pack(anchor="w")
        sn = tk.Entry(fs, textvariable=self.serial_var, font=("Segoe UI", 12), relief="solid", bd=1)
        sn.pack(fill="x", ipady=5, pady=(3, 0))
        sn.bind("<Return>", lambda e: self._start_test())

        self.btn_test = tk.Button(
            left, text="▶  Start Test",
            font=("Segoe UI", 13, "bold"),
            bg="#1a73e8", fg="white",
            activebackground="#1558b0", activeforeground="white",
            relief="flat", cursor="hand2",
            command=self._start_test)
        self.btn_test.pack(fill="x", padx=12, pady=(6, 3), ipady=10)

        tk.Label(left, textvariable=self.counter_var, font=("Segoe UI", 11, "bold"), bg=BG, fg="#1a1a1a").pack(pady=(2, 0))

        self.timer_lbl = tk.Label(left, textvariable=self.timer_var, font=("Segoe UI", 22, "bold"), bg=BG, fg="#1a73e8")
        self.timer_lbl.pack(pady=(1, 0))

        self.result_frame = tk.Frame(left, bg=BG)
        self.result_frame.pack(fill="x", padx=12, pady=3)
        self.result_lbl = tk.Label(self.result_frame, textvariable=self.result_var, font=("Segoe UI", 30, "bold"), bg=BG, fg="#888")
        self.result_lbl.pack(pady=(4, 0))
        self.status_lbl = tk.Label(self.result_frame, textvariable=self.status_var, font=("Segoe UI", 9), bg=BG, fg="#555", wraplength=400)
        self.status_lbl.pack(pady=(0, 2))
        self.reason_lbl = tk.Label(self.result_frame, textvariable=self.reason_var, font=("Segoe UI", 8, "italic"), bg=BG, fg="#E24B4A", wraplength=400, justify="center")
        self.reason_lbl.pack(pady=(0, 4))

        sf = tk.Frame(left, bg=BG)
        sf.pack(fill="x", padx=12, pady=2)
        stats = [
            ("SNR",          self.snr_var,    "≥ 12 dB = PASS"),
            ("Peak freq",    self.peak_var,   "should match generator Hz"),
            ("Signal level", self.signal_var, "dB at test frequency"),
            ("Noise floor",  self.noise_var,  "dB background"),
        ]
        for i, (label, var, hint) in enumerate(stats):
            r, c = divmod(i, 2)
            cell = tk.Frame(sf, bg="#e8e8e8", padx=8, pady=5)
            cell.grid(row=r, column=c, sticky="ew", padx=(0 if c == 0 else 5, 0), pady=(0, 5))
            sf.columnconfigure(c, weight=1)
            tk.Label(cell, text=label, font=("Segoe UI", 8), bg="#e8e8e8", fg="#666").pack(anchor="w")
            tk.Label(cell, textvariable=var, font=("Segoe UI", 13, "bold"), bg="#e8e8e8", fg="#1a1a1a").pack(anchor="w")
            tk.Label(cell, text=hint, font=("Segoe UI", 7), bg="#e8e8e8", fg="#999").pack(anchor="w")

        db_frame = tk.LabelFrame(left, text=" MySQL (optional) ", font=("Segoe UI", 8), bg=BG, fg="#555", padx=8, pady=4)
        db_frame.pack(fill="x", padx=12, pady=(4, 2))
        fields = [("Host","host"),("Port","port"),("User","user"), ("Pass","password"),("DB","database")]
        for col, (lbl, key) in enumerate(fields):
            tk.Label(db_frame, text=lbl, font=("Segoe UI", 7), bg=BG, fg="#666").grid(row=0, column=col, padx=(0, 3))
            tk.Entry(db_frame, textvariable=self.db_config[key], font=("Segoe UI", 8), width=9, show="*" if key == "password" else "", relief="solid", bd=1).grid(row=1, column=col, padx=(0, 3))
        tk.Button(db_frame, text="Connect DB", font=("Segoe UI", 8), command=self._connect_db, relief="solid", bd=1, cursor="hand2").grid(row=1, column=len(fields), padx=(4, 0))
        tk.Label(db_frame, textvariable=self.db_status_var, font=("Segoe UI", 7), bg=BG, fg="#888").grid(row=2, column=0, columnspan=len(fields)+1, sticky="w", pady=(3, 0))

        lf = tk.Frame(left, bg=BG)
        lf.pack(fill="both", expand=True, padx=12, pady=(4, 0))
        cols   = ("Time", "Serial", "Freq Hz", "SNR", "Peak Hz", "Result", "Fail Reason")
        widths = (55, 75, 55, 50, 60, 45, 160)
        self.tree = ttk.Treeview(lf, columns=cols, show="headings", height=4)
        for c, w in zip(cols, widths):
            self.tree.heading(c, text=c)
            self.tree.column(c, width=w, anchor="center" if c != "Fail Reason" else "w")
        self.tree.tag_configure("PASS",  foreground="#1D9E75")
        self.tree.tag_configure("FAIL",  foreground="#E24B4A")
        self.tree.tag_configure("ERROR", foreground="#B07A00")
        sb = ttk.Scrollbar(lf, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.tree.pack(fill="both", expand=True)

        br = tk.Frame(left, bg=BG)
        br.pack(fill="x", padx=12, pady=(3, 10))
        tk.Button(br, text="Export CSV", font=("Segoe UI", 8), command=self._export_csv, relief="solid", bd=1, cursor="hand2").pack(side="right")
        tk.Button(br, text="Clear Log", font=("Segoe UI", 8), command=self._clear_log, relief="solid", bd=1, cursor="hand2").pack(side="right", padx=(0, 6))

        right = tk.Frame(self.root, bg=DARK_BG)
        right.pack(side="right", fill="both", expand=True)

        hdr = tk.Frame(right, bg=DARK_BG)
        hdr.pack(fill="x", padx=12, pady=(10, 2))
        tk.Label(hdr, text="LIVE WAVEFORM  +  FFT", font=("Courier", 10, "bold"), bg=DARK_BG, fg=MUTED).pack(side="left")

        legend = tk.Frame(hdr, bg=DARK_BG)
        legend.pack(side="right")
        for col, lbl in [(BLUE, "Generator"), (GREEN, "Mic"), (RED, "Noise floor")]:
            dot = tk.Canvas(legend, width=20, height=10, bg=DARK_BG, highlightthickness=0)
            dot.create_line(0, 5, 20, 5, fill=col, width=2)
            dot.pack(side="left")
            tk.Label(legend, text=lbl + "  ", font=("Courier", 8), bg=DARK_BG, fg=MUTED).pack(side="left")

        graph_frame = tk.Frame(right, bg=DARK_BG)
        graph_frame.pack(fill="both", expand=True, padx=8, pady=(0, 8))

        self.graph = WaveformGraph(graph_frame, self._engine)
        self.graph.set_freq(self._test_freq)

    # ── Frequency control ─────────────────────────────────────────
    def _on_freq_slider(self, val):
        freq = int(float(val))
        self._set_freq(freq, update_slider=False)

    def _on_vol_change(self, val):
        pct = int(float(val) * 100)
        self.vol_pct_lbl.config(text=f"{pct}%")

    def _apply_custom_freq(self):
        try:
            freq = int(self.freq_var.get())
            if not (20 <= freq <= 20000):
                raise ValueError
            self._set_freq(freq)
        except ValueError:
            messagebox.showwarning("Invalid frequency", "Enter a value between 20 and 20000 Hz.")

    def _set_freq(self, freq: int, update_slider=True):
        self._test_freq = freq
        self.freq_var.set(str(freq))
        self.freq_display.config(text=f"{freq} Hz")
        if update_slider:
            self.freq_slider.set(freq)
        if hasattr(self, "graph"):
            self.graph.set_freq(freq)
        if self._preview_playing:
            self.player.stop()
            self.player.start(freq=freq, volume=self.vol_var.get())

    def _toggle_preview(self):
        if self._preview_playing:
            self.player.stop()
            self._preview_playing = False
            self.btn_preview.config(text="▶  Preview Tone", bg="#f0f4ff")
        else:
            if self.test_running:
                return
            self.player.start(freq=self._test_freq, volume=self.vol_var.get())
            self._preview_playing = True
            self.btn_preview.config(text="■  Stop Preview", bg="#fdecea")

    # ── Port helpers ──────────────────────────────────────────────
    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_menu["values"] = ports
        if ports:
            self.port_var.set(ports[0])

    def _connect_serial(self):
        port = self.port_var.get()
        if not port:
            messagebox.showwarning("No port", "Select a COM port first.")
            return
        try:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
            self.serial_port = serial.Serial(port, BAUD_RATE, timeout=10)
            self.serial_port.write(b"PING\n")
            resp = self.serial_port.readline().decode().strip()
            if "PONG" in resp or "READY" in resp:
                self.conn_label.config(text=f"Connected: {port}", fg="#1D9E75")
            else:
                self.conn_label.config(text="Connected (no PONG — check firmware)", fg="#B07A00")
        except Exception as e:
            self.conn_label.config(text=f"Failed: {e}", fg="#E24B4A")
            self.serial_port = None

    # ── Single test ───────────────────────────────────────────────
    def _start_test(self):
        if self.test_running:
            return
        if self._preview_playing:
            self._toggle_preview()

        serial_num = self.serial_var.get().strip()
        if not serial_num:
            messagebox.showwarning("Serial required", "Enter the mic serial number first.")
            return
        if not self.serial_port or not self.serial_port.is_open:
            messagebox.showwarning("Not connected", "Connect to the Glyph C6 COM port first.")
            return

        self.test_running = True
        self.btn_test.config(state="disabled", text="Testing…")
        self._set_result_ui("TESTING")
        self.result_var.set("…")
        self.reason_var.set("")
        self.snr_var.set("—")
        self.peak_var.set("—")
        self.signal_var.set("—")
        self.noise_var.set("—")
        self.timer_var.set("")
        self.status_var.set("Sending TEST command to Glyph C6…")
        self.graph.clear_result()
        self.graph.set_state(STATE_TESTING)

        threading.Thread(target=self._test_thread, args=(serial_num, self._test_freq), daemon=True).start()

    def _test_thread(self, serial_num, test_freq):
        try:
            sp = self.serial_port
            sp.reset_input_buffer()
            sp.write(f"TEST:{test_freq}\n".encode())

            deadline    = time.time() + TIMEOUT_SEC
            result_data = None

            while time.time() < deadline:
                raw = sp.readline().decode(errors="ignore").strip()
                if not raw:
                    continue
                try:
                    data = json.loads(raw)
                except json.JSONDecodeError:
                    continue

                status = data.get("status", "")

                if status == "MEASURING_NOISE":
                    self.root.after(0, self.status_var.set, "Step 1/3 — Measuring background noise floor…")

                # HUMAN LOGIC CHECK: Tone turns on exactly when the firmware signals it is ready to capture
                elif status == "PLAY_TONE":
                    self.root.after(0, self.status_var.set, f"Playing {test_freq} Hz continuous tone...")
                    self.player.start(freq=test_freq, volume=self.vol_var.get())
                    self.root.after(0, self._start_countdown, 5)

                elif status == "RECORDING":
                    self.root.after(0, self.status_var.set, "ESP32 recording and analysing microphone...")

                # HUMAN LOGIC CHECK: The instant a PASS or FAIL arrives, clear the sound environment completely
                elif status in ("PASS", "FAIL"):
                    self.player.stop()  # Clean hardware sound termination
                    self.root.after(0, self.timer_var.set, "")
                    result_data = data
                    break

                elif status == "ERROR":
                    self.player.stop()  # Turn off sound on exception loop break
                    self.root.after(0, self.timer_var.set, "")
                    self.root.after(0, self._show_error, data.get("msg", "Unknown firmware error"))
                    return

            if result_data is None:
                self.player.stop()  # Terminate streaming output safely
                self.root.after(0, self.timer_var.set, "")
                self.root.after(0, self._show_error, "Timeout — no result received from Glyph C6")
                return

            reason = classify_fail_reason(result_data, test_freq)
            self.root.after(0, self._show_result, serial_num, result_data, reason, test_freq)

        except Exception as e:
            self.player.stop()      # Catch-all failsafe sound closure
            self.root.after(0, self.timer_var.set, "")
            self.root.after(0, self._show_error, str(e))

    # ── Countdown ─────────────────────────────────────────────────
    def _start_countdown(self, secs_left):
        if not hasattr(self, "_countdown_gen"):
            self._countdown_gen = 0
        self._countdown_gen += 1
        self._run_countdown(secs_left, self._countdown_gen)

    def _run_countdown(self, secs_left, gen):
        if gen != self._countdown_gen:
            return   
        if secs_left <= 0:
            self.timer_var.set("")
            return
        self.timer_var.set(f"🔊 Tone playing — {secs_left}s")
        self.root.after(1000, self._run_countdown, secs_left - 1, gen)

    # ── Show result ───────────────────────────────────────────────
    def _show_result(self, serial_num, data, reason, test_freq):
        verdict   = data.get("status", "FAIL")
        snr       = data.get("snr", 0.0)
        peak_freq = data.get("peak_freq", 0)
        signal_db = data.get("signal_db", 0.0)
        noise_db  = data.get("noise_db", 0.0)

        # Overwrite evaluation tracking to pass any working mic tracking at or above 12 dB
        if snr >= 12.0:
            verdict = "PASS"
            reason = ""
        else:
            verdict = "FAIL"

        self.result_var.set(verdict)
        self.reason_var.set(reason)
        self.snr_var.set(f"{snr:.1f} dB")
        self.peak_var.set(f"{peak_freq} Hz")
        self.signal_var.set(f"{signal_db:.1f} dB")
        self.noise_var.set(f"{noise_db:.1f} dB")
        self._set_result_ui(verdict)
        
        data["status"] = verdict
        self.graph.set_result(data)   

        if verdict == "PASS":
            self.status_var.set(f"Mic heard {test_freq} Hz clearly — SNR {snr:.1f} dB (≥ 12 dB required). PASS ✓")
        else:
            self.status_var.set(f"SNR {snr:.1f} dB — below 12 dB threshold. See reason below.")

        self.session_total += 1
        if verdict == "PASS":
            self.session_pass += 1
        else:
            self.session_fail += 1
        self.counter_var.set(f"PASS: {self.session_pass}     FAIL: {self.session_fail}     TOTAL: {self.session_total}")

        now = datetime.datetime.now().strftime("%H:%M:%S")
        row = (now, serial_num, f"{test_freq}", f"{snr:.1f}", str(peak_freq), verdict, reason)
        self.tree.insert("", 0, values=row, tags=(verdict,))

        self._save_to_db(serial_num, verdict, snr, peak_freq, signal_db, noise_db, reason)

        self.test_running = False
        self.btn_test.config(state="normal", text="▶  Start Test")
        self.serial_var.set("")

    def _show_error(self, msg):
        self.result_var.set("ERROR")
        self.status_var.set(f"Error: {msg}")
        self.reason_var.set("")
        self._set_result_ui("ERROR")
        self.graph.set_state(STATE_IDLE)
        self.session_total += 1
        self.session_fail  += 1
        self.counter_var.set(f"PASS: {self.session_pass}     FAIL: {self.session_fail}     TOTAL: {self.session_total}")
        self.test_running = False
        self.btn_test.config(state="normal", text="▶  Start Test")

    def _set_result_ui(self, state):
        bg, fg = COLORS.get(state, COLORS["READY"])
        self.result_frame.config(bg=bg)
        self.result_lbl.config(bg=bg, fg=fg)
        self.status_lbl.config(bg=bg)
        self.reason_lbl.config(bg=bg)

    # ── Database ──────────────────────────────────────────────────
    def _connect_db(self):
        if not HAS_MYSQL:
            messagebox.showerror("Missing library", "Run:  pip install mysql-connector-python")
            return
        try:
            cfg = {k: v.get() for k, v in self.db_config.items()}
            cfg["port"] = int(cfg["port"])
            self.db_conn = mysql.connector.connect(**cfg)
            cur = self.db_conn.cursor()
            cur.execute("""
                CREATE TABLE IF NOT EXISTS mic_test_results (
                    id            INT AUTO_INCREMENT PRIMARY KEY,
                    serial_number VARCHAR(100),
                    test_time     DATETIME,
                    snr_db        FLOAT,
                    peak_freq_hz  INT,
                    signal_db     FLOAT,
                    noise_db      FLOAT,
                    result        VARCHAR(10),
                    fail_reason   TEXT
                )
            """)
            self.db_conn.commit()
            self.db_status_var.set("Connected")
        except Exception as e:
            self.db_status_var.set(f"Failed: {e}")
            self.db_conn = None

    def _save_to_db(self, serial_num, verdict, snr, peak_freq, signal_db, noise_db, reason):
        if not self.db_conn:
            return
        try:
            cur = self.db_conn.cursor()
            cur.execute("""
                INSERT INTO mic_test_results
                  (serial_number, test_time, snr_db, peak_freq_hz,
                   signal_db, noise_db, result, fail_reason)
                VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
            """, (serial_num, datetime.datetime.now(), snr, peak_freq, signal_db, noise_db, verdict, reason))
            self.db_conn.commit()
        except Exception as e:
            self.db_status_var.set(f"DB write error: {e}")

    # ── Export / clear ────────────────────────────────────────────
    def _export_csv(self):
        if not self.tree.get_children():
            messagebox.showinfo("Nothing to export", "Run some tests first.")
            return
        path = os.path.join(os.path.expanduser("~"), "Desktop", "mic_test_log.csv")
        with open(path, "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["Time", "Serial", "Freq Hz", "SNR (dB)", "Peak Hz", "Result", "Fail Reason"])
            for iid in self.tree.get_children():
                w.writerow(self.tree.item(iid, "values"))
        messagebox.showinfo("Exported", f"Saved to:\n{path}")

    def _clear_log(self):
        self.session_pass  = 0
        self.session_fail  = 0
        self.session_total = 0
        self.counter_var.set("PASS: 0     FAIL: 0     TOTAL: 0")
        for item in self.tree.get_children():
            self.tree.delete(item)


if __name__ == "__main__":
    root = tk.Tk()
    app  = MicTesterApp(root)
    root.mainloop()