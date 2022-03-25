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

### Receiving CAN Messages

**Function:** ```CAN_RX_ISR()```  

**Purpose:**  

* Copies incoming CAN messages in the CAN buffer to an internal - larger - buffer, to be handled later by `decodeTask()`  

**Implementation:**  Interrupt, executing whenever a new CAN message is received. An interrupt is chosen as the receive buffer is small and needs to be emptied often to avoid loss of CAN messages.

**Minimum initiation time:** No set minimum initiation time, runs as often as needed. Frequency limit of this interrupt is determined by the maximum execution time and the time duration of CAN Messages, which is `0.7ms`

**Maximum execution time:** Requires a CAN Message to be received from another board, which would need to flood the CAN bus. Maximum execution time of one iteration is likely below the length of a CAN message and not easily quantifiable.

## Critical instant analysis of the rate monotonic scheduler

From the minimum initiation and maximum execution times obtained in the last section, the critical analysis is calculated using the formula provided in the lecture notes. The lowest priority task is updating the display. The minimum initiation and maximum execution time are summarised below, in ascending order:

1. CAN_RX_ISR - Not quantified & 0.7ms
1. scanKeysTask - 73.65us & 20ms
1. decodeTask - 0.76us & 25ms
1. sampleISR - 12.17us & 45.15ms
1. updateDisplayTask - 17.07ms & 100ms

Therefore, total latency is slightly over 23.07ms. The exact latency could not be calculated, as the exact worst case execution time of CAN_RX_ISR is not known. However, as the specified latency is less than 100ms, the schedule will work.

## CPU Resource Usage

TO BE ADDED - After system is completed; see [this](https://edstem.org/us/courses/19499/discussion/1300057) link

## Shared data structures

Shared data structures:

* code(currentStepSize), safe access guaranteed using code(std::atomic<uint32_t>)  
* code(keyArray), each element within the array is of type code(std::atomic<uint8_t>)  
* msgInQ, handled by FreeRTOS  
* code(RX_Message), handled by code(std::atomic_flag)  

It was desiced to use C++ code(std::atomic), as it is easier to use and implement, while providing the same functionality as a mutex. According to the documentation: *"Each instantiation and full specialization of the std::atomic template defines an atomic type. If one thread writes to an atomic object while another thread reads from it, the behavior is well-defined (see memory model for details on data races). In addition, accesses to atomic objects may establish inter-thread synchronization and order non-atomic memory accesses as specified by std::memory_order.*" - [CPP Reference "std::atomic"](https://en.cppreference.com/w/cpp/atomic/atomic)

TODO - Expand on the memory model for std::atomic.

TODO - Explain how FreeRTOS handles msgInQ.

TODO - Once CAN is completed, expand on the protection of RX_Message

## Analysis of inter-task blocking dependencies
