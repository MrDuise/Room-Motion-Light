## 2026-11-04: UART loopback
**Goal:** Prove UART2 works with TX jumpered to RX.
**Did:** Configured UART2, sent a string every 2 s, read it back with the async event handler.
**Observed:** Clean echo every cycle. Jumper removed: only "Sent" lines. Jumper replugged: garbage bytes, then every read offset by 30 bytes.
**Surprises / bugs:** Reads don't realign after stray bytes. UART is a stream with no message boundaries. Once the garbage bytes cleaned up, I was surpised at the offset.
**Decisions:** Chose the ESP-IDF's drivers UART event queue over a blocking read, so framing errors and overflows reset the parser instead of reaching it as data. See DR-03.