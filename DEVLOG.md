## Build steps
✅ Toolchain and hello world
✅ Dummy tasks and queue
✅ UART loopback 
✅ Wire up the LD2410 and print raw bytes
✅ Write the parser from the datasheet’s frame table
✅ LED strip on its own
Combine everything


## 2026-11-24: LED integration
**Goal:** Integrate the LED control with the state control task.
**Did:** Added `ledOn()` and `ledOff()` functions, called them in the state control task based on the sensor state.
**Observed:** LED turns on when motion is detected and off when no motion is detected.
**Surprises / bugs:** Background noise or reflections can sometimes trigger the LED even when no actual motion is present. This highlights the need for careful tuning of the sensor's sensitivity and distance gates to minimize false positives.
**Decisions:** LED control is now integrated with the state control, will be updated to be designed to handle false positives more gracefully based on energy thresholds to determine actual motion versus background noise.

## 2026-11-18: LC2410C sensor task
**Goal:** Read the LC2410C sensor data and parse it into a frame.
**Did:** Implemented a task that reads from the UART event queue, parses the data, and prints the parsed frame.
**Observed:** The parser works, but the task is not yet integrated with the rest of the system. The task currently just prints the parsed frame to the console.
**Surprises / bugs:** When testing moving/stationary detection, the sensor sometimes sends a "moving" frame when the room is empty. This may be due to reflections or other environmental factors.
**Decisions:** Further testing and fine tuning of the sensor's sensitivity and distance gates is needed to reduce false positivies. Once that is done, a simple task will be added to turn on a light when the sensor detects motion, and turn it off after a timeout when no motion is detected. See DR-03 for the UART event queue decision.

## 2026-11-04: UART loopback
**Goal:** Prove UART2 works with TX jumpered to RX.
**Did:** Configured UART2, sent a string every 2 s, read it back with the async event handler.
**Observed:** Clean echo every cycle. Jumper removed: only "Sent" lines. Jumper replugged: garbage bytes, then every read offset by 30 bytes.
**Surprises / bugs:** Reads don't realign after stray bytes. UART is a stream with no message boundaries. Once the garbage bytes cleaned up, I was surpised at the offset.
**Decisions:** Chose the ESP-IDF's drivers UART event queue over a blocking read, so framing errors and overflows reset the parser instead of reaching it as data. See DR-03.





