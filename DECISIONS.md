## DR-03: How the sensor task waits for UART data
Options: A) blocking read with timeout, B) driver event queue, C) polling
Chosen: B
Why: I want transport errors handled separately from parsing, and I want to count them while tuning.
Cost: more code, since every event type needs a handler.
Revisit if: the extra handling turns out to be noise and A would be simpler.