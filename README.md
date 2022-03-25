# ELEC60013 Embedded Systems - Music Synthesizer Documentation
The following document outlines the design, implementation and real-time dependecies of a music synthesizer for the 2nd coursework of the Embedded Systems module.

## System workload
The sytem at hand executes several real-time tasks, including:  
* Scannng the key matrix
* CAN decoding task  
* Updating the display  
* Generating sound  

### Scanning the key matrix  
**Function:** ```void scanKeysTask(void *pvParameters))```     

**Purpose:**  
* Decode the key matrix, obtaining the states of the keys, knobs and joysticks  
* Obtain the currently selected note from the decoded key matrix  
* Select the appopriate step size for the currently selected note  
* Transmit the currently selected note, along side its state (pressed or released) and octave using CAN  
* Obtain any changes in the knobs from the decoded key matrix, corresponding to volume, octave or waveform  

**Implementation:** Thread 
**Minimum initiation time:** 50ms.  
**Maximum execution time:** TO BE COMPLETED.  
**Priority:** Highest, as obtaining the note, volume and octave is required for playing the sound, transmitting over CAN and displaying on the screen. All other tasks depend on the results obtained from scanning the key matrix.

### Decode task
**Function:** ```void decodeTask(void *pvParameters))```     

**Purpose:**  
* Receive the incoming CAN message  
* Decide whether the key was pressed, released or the message originated from the main synthesizer  
* Update the array of currently active notes
* Set the current step size, with respect to the currently active note.

**Implementation:** Thread 
**Minimum initiation time:** 70ms, assuming an average speed of 14 notes per second for professional pianist, as explained in the following Journal article - https://www.thejournal.ie/worlds-fastest-pianist-1780480-Nov2014/. This thread is an example for which the initiation time is not set internally, which was the case for ```void scanKeysTask(void *pvParameters))```. Instead, this task is a thread, whose initiation time depends on the purpose of the system - in this case, the speed of the pianist.
**Maximum execution time:** TO BE COMPLETED.  
**Priority:** Medium, due to a lower initiation time, compared to scanning the key matrix. Furthermore, in order to receive CAN messages, they had to be transmitted first - which is handled in ```scanKeyTask```.

### Updating the display  
**Function:** code(displayUpdateTask(void *pvParameters)  

**Purpose:**  
* Display the currently selected note (string)  
* Display the current volume (int) or an animated icon of the sound (XMB icon)  
* Display the current octave (int)  
* Display the current waveform (XMB icons), including sawtooth, triangle, sine and rectangle.  

**Implementation:** Thread, executing every 100ms.   
**Minimum initiation time:** 100ms. While a refresh rate of 10Hz sounds insufficient (compared to modern monitors); the display is largely static. Therefore, 100ms is a perfectly accetable design choice, without any noticeable delays.   
**Maximum execution time:** TO BE COMPLETED.  
**Priority:** Lowest, there is no point in updating the display if the current note, volume or octave have not been update - all of which take placee in the scanning key matrix task.

### Generating the sound  
**Purpose:**  
* Adds the sample rate to the accumulated value, corresponding to the desired frequency  
* Sets the output voltage, by using the accumlated value and the desired volume  
* Writes to analogue output, generating the desired sound  

**Function:** ```sampleISR()```  
**Implementation:**  Interrupt, executing with a frequency of 44.1kHz. An interrupt is chosen as responding quickly is needed for playing sound. Otherwise, delays between the key press and the sound would exist.  
**Minimum initiation time:** 22.7ms.   
**Maximum execution time:** TO BE COMPLETED.  

## Critical instant analysis of the rate monotonic scheduler  
TO BE ADDED - After system is completed.  

## CPU Resource Usage  
TO BE ADDED - After system is completed; see this link: https://edstem.org/us/courses/19499/discussion/1300057  

## Shared data structures  
Shared data structures:  
* code(currentStepSize), safe access guaranteed using code(std::atomic<uint32_t>)  
* code(keyArray), each element within the array is of type code(std::atomic<uint8_t>)  
* msgInQ, handled by FreeRTOS  
* code(RX_Message), handled by code(std::atomic_flag)  

It was desiced to use C++ code(std::atomic), as it is easier to use and implement, while providing the same functionality as a mutex. According to the documentation: *"Each instantiation and full specialization of the std::atomic template defines an atomic type. If one thread writes to an atomic object while another thread reads from it, the behavior is well-defined (see memory model for details on data races). In addition, accesses to atomic objects may establish inter-thread synchronization and order non-atomic memory accesses as specified by std::memory_order.*"(https://en.cppreference.com/w/cpp/atomic/atomic) 

TODO - Expand on the memory model for std::atomic.  
TODO - Explain how FreeRTOS handles msgInQ.   
TODO - Once CAN is completed, expand on the protection of RX_Message  

## Analysis of inter-task blocking dependencies

