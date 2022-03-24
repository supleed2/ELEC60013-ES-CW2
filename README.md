# ELEC60013 Embedded Systems - Music Synthesizer Documentation
The following document outlines the design, implementation and real-time dependecies of a music synthesizer for the 2nd coursework of the Embedded Systems module.

## System tasks
The sytem at hand executes several real-time tasks, including: 
*Reading the key matrix
*Updating the display
*Generating sound

### Reading the key matrix
**Purpose:**
*Decode the key matrix, obtaining the states of the keys, knobs and joysticks
*Obtain the currently selected note from the decoded key matrix
*Select the appopriate step size for the currently selected note
*Transmit the currently selected note, along side its state (pressed or released) using CAN
*Obtain any changes in the knobs from the decoded key matrix, corresponding to volume, octave or waveform

**Function:** code(void scanKeysTask(void *pvParameters))
**Implementation:** Thread, executing every 50ms
**Minimum initiation time:** TO BE COMPLETED.
**Maximum execution time:** TO BE COMPLETED.
**Priority:** Highest (1), as playing the appropriate sound is more important than updating the display.

### Updating the display
**Purpose:**
*Display the currently selected note (string)
*Display the current volume (int)
*Display the current octave (int)
*Display the current waveform (XMB icons)

**Function:** code(displayUpdateTask(void *pvParameters)
**Implementation:** Thread, executing every 100ms. While a refresh rate of 10Hz does not sound impressive; the display is largely static. Therefore, 100ms was perfectly accetable, without any noticeable delays.
**Minimum initiation time:** TO BE COMPLETED.
**Maximum execution time:** TO BE COMPLETED.
**Priority:** Lowest (2).

### Generating the sound
**Purpose:**
*Adds the sample rate to the accumulated value, corresponding to the desired frequency
*Sets the output voltage, by using the accumlated value and the desired volume
*Writes to analogue output, generating the desired sound

**Function:** code(sampleISR())
**Implementation:**  Interrupt executing with a frequency of 44.1kHz
**Minimum initiation time:** TO BE COMPLETED.
**Maximum execution time:** TO BE COMPLETED.

### CAN
**Purpose:**
* TO BE ADDED

**Function:**
**Implementation:** 
**Minimum initiation time:** 
**Maximum execution time:** 
**Priority:** 

## Critical instant analysis of the rate monotonic scheduler
TO BE ADDED - After system is completed.

## CPU Resource Usage
TO BE ADDED - After system is completed; see this link: https://edstem.org/us/courses/19499/discussion/1300057

## Shared data structures
Shared data structures:
*code(currentStepSize), safe access guaranteed using code(std::atomic<uint32_t>)
*code(keyArray), each element within the array is of type code(std::atomic<uint8_t>)
*msgInQ, handled by FreeRTOS
*code(RX_Message), handled by code(std::atomic_flag)

It was desiced to use C++ code(std::atomic), as it is easier to use and implement, while providing the same functionality as a mutex. According to the documentation: *"Each instantiation and full specialization of the std::atomic template defines an atomic type. If one thread writes to an atomic object while another thread reads from it, the behavior is well-defined (see memory model for details on data races). In addition, accesses to atomic objects may establish inter-thread synchronization and order non-atomic memory accesses as specified by std::memory_order."* (https://en.cppreference.com/w/cpp/atomic/atomic) 

TODO - Expand on the memory model for std:: atomic.
TODO - Explain how FreeRTOS handles msgInQ.
TODO - Once CAN is completed, expand on the protection of RX_Message

## Analysis of inter-task blocking dependencies

