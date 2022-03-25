# ELEC60013 Embedded Systems - Music Synthesizer Documentation

The following document outlines the design, implementation and real-time dependecies of a music synthesizer for the 2nd coursework of the Embedded Systems module.

## System workload

The sytem at hand executes several real-time tasks, including:

* Scannng the key matrix
* CAN decoding task  
* Updating the display  
* Generating sound  
* Receiving CAN Messages

### Scanning the key matrix

**Function:** ```void scanKeysTask(void *pvParameters)```

**Purpose:**

* Decode the key matrix, obtaining the states of the keys, knobs and joystick  
* Obtain the most recently changed note from the decoded key matrix  
* Select the appopriate step size for the most recently changed note  
* Transmit the most recently changed note, alongside its state (pressed or released) and octave using CAN  
* Obtain any changes in the knobs from the decoded key matrix, corresponding to octave, waveform, send/receive mode or volume

**Implementation:** Thread

**Minimum initiation time:** `20ms`

**Maximum execution time:** `0.74ms`

**CPU Resource Usage:** `3.68%`

**Priority:** Highest, as obtaining the note, volume and octave is required for playing the sound, transmitting over CAN and displaying on the screen. All other tasks depend on the results obtained from scanning the key matrix.

### Decode task

**Function:** ```void decodeTask(void *pvParameters)```

**Purpose:**

* Decode the incoming CAN message in the Receive Queue: `msgInQ`
* Determine if message is `key pressed`, `key released` or the announcement of the main synthesizer to put other modules into `SEND` mode.
* Update the array of currently active notes
* Set the current step size, with respect to the most recently active note.

**Implementation:** Thread

**Minimum initiation time:** The input for `decodeTask()` is a Queue, filled from within `CAN_RX_ISR()`. The resulting minimum initiation time is the size of the queue multiplied by the minimum initiation time of the function that fills the Queue. In this case: `0.7 * 36 = 25.2ms`

**Maximum execution time:** `0.76us`

**CPU Resource Usage:** `0.00304%`

**Priority:** Medium, due to a lower initiation time, compared to scanning the key matrix. Furthermore, in order to receive CAN messages, they had to be transmitted first - which is handled in ```scanKeyTask```.

### Updating the display

**Function:** `displayUpdateTask(void *pvParameters)`

**Purpose:**

* Display the currently selected note (string)
* Display the current volume level (int) or an animated icon (XMB icon)  
* Display the current octave (int)  
* Display the current waveform (XMB icons), including sawtooth, triangle, sine and square

**Implementation:** Thread, executing every 100ms.

**Minimum initiation time:** 100ms. While a refresh rate of 10Hz sounds insufficient (compared to modern monitors); the display is largely static. Therefore, 100ms is a perfectly accetable design choice, without any noticeable delays.

**Maximum execution time:** `17.01ms`

**CPU Resource Usage:** `17.01%`

**Priority:** Lowest, there is no point in updating the display if the current note, volume or octave have not been updated - all of which take place in the scanning key matrix task.

### Generating the sound

**Function:** ```sampleISR()```  

**Purpose:**  

* Adds the sample step to the accumulated value, corresponding to the desired frequency  
* Sets the output voltage, by using the accumlated value and the desired volume  
* Writes to analogue output, generating the desired sound  

**Implementation:**  Interrupt, executing with a frequency of 22kHz. An interrupt is chosen as responding quickly is needed for playing sound. Otherwise, delays between the key press and the sound would exist.

**Minimum initiation time:** 45.5ms

**Maximum execution time:** `12.17us`

**CPU Resource Usage:** `0.27%`

### Receiving CAN Messages

**Function:** ```CAN_RX_ISR()```  

**Purpose:**  

* Copies incoming CAN messages in the CAN buffer to an internal - larger - buffer, to be handled later by `decodeTask()`  

**Implementation:**  Interrupt, executing whenever a new CAN message is received. An interrupt is chosen as the receive buffer is small and needs to be emptied often to avoid loss of CAN messages.

**Minimum initiation time:** No set minimum initiation time, runs as often as needed. Frequency limit of this interrupt is determined by the maximum execution time and the time duration of CAN Messages, which is `0.7ms`

**Maximum execution time:** Requires a CAN Message to be received from another board, which would need to flood the CAN bus. Maximum execution time of one iteration is likely below the length of a CAN message and not easily quantifiable.

**CPU Resource Usage:** Not quantifiable as the execution time could not be measured.

## Critical Instant Analysis & Total CPU Usage

From the minimum initiation and maximum execution times obtained in the last section, the critical analysis is calculated using the formula provided in the lecture notes. The lowest priority task is updating the display. The minimum initiation and maximum execution time are summarised below, in ascending order:

1. CAN_RX_ISR - Not quantified & 0.7ms
1. scanKeysTask - 73.65us & 20ms
1. decodeTask - 0.76us & 25ms
1. sampleISR - 12.17us & 45.15ms
1. updateDisplayTask - 17.07ms & 100ms

Therefore, total latency is slightly over 23.07ms. The exact latency could not be calculated, as the exact worst case execution time of CAN_RX_ISR is not known. However, as the specified latency is less than 100ms, the schedule will work.

The total CPU usage is calculated by dividing the total latency by the highest initiation time. In this case the CPU usage is ~25%.

## Shared data structures

* `currentStepSize`, safe access guaranteed using `std::atomic<uint32_t>`, stores the step size of the most recently pressed key, to be used within `sampleISR()`
* `keyArray`, each element within the array is of type `std::atomic<uint8_t>`, stores the current state of the key / encoder matrix
* `msgInQ`, handled by FreeRTOS, pointer to the next item in the received CAN message queue

It was decided to use C++ `std::atomic`, as it is easier to use and implement, while providing the same functionality as a mutex. According to the documentation: *"Each instantiation and full specialization of the std::atomic template defines an atomic type. If one thread writes to an atomic object while another thread reads from it, the behavior is well-defined (see memory model for details on data races). In addition, accesses to atomic objects may establish inter-thread synchronization and order non-atomic memory accesses as specified by std::memory_order.*" - [CPP Reference "std::atomic"](https://en.cppreference.com/w/cpp/atomic/atomic)

`std::atomic` makes use of atomic builtins within GCC in order to prevent data races from occuring when there are simultaneous accesses of a variable from different threads. The result of wrapping our global variables in `std::atomic<>` is that any access to these variables is always atomic, so data corruption is prevented. In some cases, such as `keyArray`, the array may be partially updated. This is not an issue for data integrity or program operation, as the keyArray update will simply be slightly delayed. This delay is unlikely to be noticable to the user.

`msgInQ` is used as a FIFO buffer for CAN messages, to supplement the small buffer of the CAN system and allow for more messages to be retained, increasing the required minimum initiation time.

## Analysis of inter-task blocking dependencies

## Advanced features

Several advanced features were implemented, including:

* Multiple waveforms
* User-friendly icons
* Automatic sender mode configuration using reciever module
* Finer volume control
* Storing multiple key presses

### Multiple waveforms

Our system implements several waveforms - sawtooth, triangle, sine, square. Sawtooth was implemented first, by incrementing the step size and waiting for overflow to occur, reseting the waveform. The triangle function was obtained by modifying the sawtooth function. The absolute value of the sawtooth function is taken, shifted to remove the DC offset and doubled to resore the original size. The sine function was implemented using a sine table. Rectangular function was implemented by taking the sign of the sawtooth function.

### User-friendly icons

Using XMB icons, the display was made more user-friednly, containing a volume icon, that is animated to the current volume level. Furthermore, by pressing the knob, this icon is toggled to display the volume as an integer. Finally, several icons are used to display the current waveform.

### Automatic sender mode configuration using receiver module

When one of the keyboards is set to be the receiver, the encoder automatically sets the other keyboards to be CAN senders.

### Finer volume control

A finer volume control method was implemented, allowing the user to set a more accurate volume. The resolution is now set to 20 steps, utilsing some additional algebra.

### Storing multiple key presses

The original code was modified to store multiple keys as the current key. This was achieved by using an array. Currently, only the most recently pressed key is played - however, given the array, this is easily modified to play more than note.
