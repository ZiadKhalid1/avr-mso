# ATmega328P Mixed Signal Oscilloscope (MSO) & Logic Analyzer

An embedded Mixed Signal Oscilloscope (MSO) and high-speed Logic Analyzer implemented on the 8-bit **Microchip ATmega328P** (16 MHz). This project combines single-channel analog waveform acquisition with a 6-channel synchronized digital logic analyzer, streaming real-time acquisition buffers to a host PC over an optimized **1 Mbps** UART link.

The firmware is designed with a strict, layered **Microcontroller Abstraction Layer (MCAL)** architecture prioritizing deterministic execution, cycle-accurate timing, and a **zero-SRAM footprint** for driver internals.

---

## 1. System Architecture & Core Specifications

### Performance Metrics

* **Core Clock ($F_{CPU}$):** 16.000 MHz (External Crystal Oscillator).
* **Digital Logic Sampling Rate:** Up to **5.33 MS/s** (3 CPU cycles/sample in unrolled capture mode).
* **Analog Acquisition Rate:** Up to **76.9 kS/s** (8-bit resolution, ADC prescaler configured to 16).
* **Host Interface:** 1,000,000 baud (1 Mbps), 8N1, Double-Speed (`U2X0 = 1`), **0.0% theoretical baud rate error**.
* **Internal Buffer Footprint:** Up to 1024 bytes statically allocated for sample capture (out of 2048 bytes total SRAM).
* **Driver Dynamic Memory Allocation:** 0 bytes (no heap, zero `.bss`/`.data` allocations for driver dispatch tables).

---

## 2. Hardware Architecture & Channel Mapping

| Subsystem | Pin Name | Physical Pin | Register / Bit | Role / Configuration |
| --- | --- | --- | --- | --- |
| **Logic Channel 0** | D8 | Pin 14 | `PINB0`<br> | Digital Logic Analyzer input (Ch 0) |
| **Logic Channel 1** | D9 | Pin 15 | `PINB1`<br> | Digital Logic Analyzer input (Ch 1) / OC1A |
| **Logic Channel 2** | D10 | Pin 16 | `PINB2`<br> | Digital Logic Analyzer input (Ch 2) |
| **Logic Channel 3** | D11 | Pin 17 | `PINB3`<br> | Digital Logic Analyzer input (Ch 3) |
| **Logic Channel 4** | D12 | Pin 18 | `PINB4`<br> | Digital Logic Analyzer input (Ch 4) |
| **Logic Channel 5** | D13 | Pin 19 | `PINB5`<br> | Digital Logic Analyzer input (Ch 5) / Status LED |
| **Analog Probe (CH0)** | A0 | Pin 23 | `ADC0` (`PC0`) | Analog Oscilloscope Channel |
| **Analog Comparator +** | D6 | Pin 12 | `AIN0` (`PD6`) | Hardware Trigger Positive Input (or Internal 1.1V Bandgap) |
| **Analog Comparator -** | D7 | Pin 13 | `AIN1` (`PD7`) | Hardware Trigger Negative Input (Muxed via `ACME`) |
| **Digital Trigger** | D2 | Pin 4 | `INT0` (`PD2`) | External Interrupt Hardware Trigger input |
| **Test Signal Out** | D3 | Pin 5 | `OC2B` (`PD3`) | Calibration Waveform Output (Timer2 PWM/CTC) |
| **Host Communication** | D1 (TX) | Pin 3 | `TXD` (`PD1`) | UART Transmit to PC (1 Mbps) |
| **Host Communication** | D0 (RX) | Pin 2 | `RXD` (`PD0`) | UART Receive from PC (1 Mbps) |

---

## 3. MCAL Module Breakdown

The firmware follows a clean, decoupled layout:

* `*_Interface.h`: Exposed public APIs, configuration types, and enums.


* `*_Private.h`: Hardware register addresses, bit definitions, and internal masks.


* `*_Program.c`: Layer implementation executing direct register manipulations.


* `*_Config.h`: Compile-time hardware tailoring.

```
├── MCAL/
│   ├── DIO/            # Single-cycle I/O and Port manipulation
│   ├── ADC/            # High-speed free-running / triggered 8-bit conversions
│   ├── UART/           # 1 Mbps zero-overhead block streaming
│   ├── TIMER1/         # 16-bit sampling timebase & ICU capture
│   ├── TIMER2/         # 8-bit calibration waveform generator
│   ├── AC/             # Analog Comparator for zero-latency hardware trigger
│   └── EXTI/           # External Interrupt for sub-microsecond edge detection
├── HAL/
│   └── SIGNAL_GEN/     # Calibration signal synthesis (PWM / Sine LUT)
├── ENGINE/
│   ├── Capture_Engine/ # Cycle-counted assembly & interleaved acquisition loops
│   └── Trigger_Core/   # Multi-source trigger validation & state machine
└── LIB/
    ├── STD_TYPES.h     # Fixed-width primitive definitions
    └── BIT_MATH.h      # Register bitwise manipulation macros

```

### Module Responsibilities

* **DIO (Digital Input/Output):**
Constructed without lookup tables in SRAM. Features single-cycle hardware toggles (`PINx` write) and inline assembly-based port reads (`in` instruction).


* **UART (Universal Asynchronous Receiver/Transmitter):**
Operates at $1\text{ Mbps}$ with $0.0\%$ divider error using the 16 MHz system clock ($UBRR0 = 1, U2X0 = 1$). Provides `UART_SendBuffer()` to stream contiguous SRAM buffers without call-stack overhead by polling `UDRE0` directly in a tight pointer-walk loop.
* **TIMER1 (16-bit Timebase & ICU):**
Configured in CTC Mode (Clear Timer on Compare Match) to govern exact sampling periods ($10\text{ kS/s}$, $20\text{ kS/s}$, $50\text{ kS/s}$, etc.) without timing jitter. Its Input Capture Register (`ICR1`) routes directly to the Analog Comparator to timestamp trigger events down to a single clock tick ($62.5\text{ ns}$).
* **ADC (Analog-to-Digital Converter):**
Operates in left-adjusted 8-bit mode (`ADLAR = 1`) so that conversion results are read via a single 8-bit register access (`ADCH`), dropping read latency and conserving memory. The clock prescaler is overclocked to $500\text{ kHz}$ – $1\text{ MHz}$ to exceed typical conversion rates for dynamic signal tracking.
* **AC (Analog Comparator):**
Provides pure hardware-level analog triggering. It monitors the input waveform against either an external threshold or the internal $1.1\text{ V}$ bandgap reference, initiating acquisition without CPU polling latency.
* **TIMER2 (8-bit Generator):**
Operates independently in Fast PWM or CTC mode to supply reference square waves or clock a Sine-Wave Lookup Table (LUT), providing an on-board calibration signal for probing and verification.

---

## 4. Acquisition Modes

### Mode 1: Hybrid MSO Capture (Interleaved Analog + Digital)

* Captures 1 analog channel (`ADC0`) and 6 logic analyzer channels (`PB0`–`PB5`).


* Trigger engine detects the threshold via AC or EXTI.
* An interleaved assembly/C loop starts ADC conversion, reads the instant digital state from `PINB` in 1 clock cycle, awaits ADC completion, reads `ADCH`, and commits the packed sample pair to SRAM.



### Mode 2: Ultra-Fast Digital Logic Analyzer (Pure LA)

* Captures 6 logic channels simultaneously on Port B (`PB0`–`PB5`).


* Bypasses the ADC completely.
* Executes an unrolled capture loop:
```assembly
in   r24, 0x03    ; 1 cycle: Read PINB (Pins 8-13) directly from I/O space
st   X+, r24      ; 2 cycles: Store sample to SRAM buffer and increment pointer

```


* Achieves a real-time sampling rate of **$5.33\text{ MS/s}$** ($16\text{ MHz} / 3\text{ cycles}$).

---

## 5. Host Communication Protocol

Samples are streamed as binary frames to minimize transmission overhead:

```
+---------------+---------------+--------------------+---------------+
| SOF (2 Bytes) | Length (2 B)  | Payload Data       | EOF (1 Byte)  |
| 0xAA  0x55    | Big-Endian    | [Analog] / [Logic] | 0xFF          |
+---------------+---------------+--------------------+---------------+

```

* **Transmission Speed:** $1,000,000\text{ bps}$.
* **Buffer Dump Time:** A full 512-byte capture buffer transmits in **~5.1 ms**, delivering smooth screen update rates on the host application without blocking future trigger detection.

---

## 6. Build & Toolchain Requirements

* **Toolchain:** `avr-gcc` (v7.3.0 or higher), `avr-libc`, `avrdude`
* **Optimization Flags:** `-O2` or `-Os` (mandatory to ensure inline functions and cycle-counted loops maintain deterministic timing)
* **Target Hardware:** Microchip ATmega328P (Arduino Uno, Nano, or bare-metal breadboard system running at 16.0 MHz / 5V)
* **Host Software Interface:** Python 3.x with `pyqtgraph` / `pyserial` or standard terminal emulator configured for 1,000,000 baud.