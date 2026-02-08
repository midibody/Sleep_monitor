Serial Command Interface
---
The software exposes a lightweight command line over the USB serial port. Commands are single-letter, optionally followed by parameters (no spaces required). 
They are mainly used to inspect, export, and manage the internal flash “circular storage” datasets, plus a few debug / test controls.

They can be used from the Arduino IDE terminal, or from the small GUI (sleepMonitor.py) integrated in the project.

**Commands list:**

`?` prints the list of available serial commands.

`p` loads a dataset from flash and immediately outputs it in CSV format.

`a` inspects the flash storage structure.

`d` displays the currently loaded dataset in a human-readable table format.

`c` outputs the currently loaded dataset in CSV format for external analysis.

`v` displays stored log or event records associated with the current dataset.

`r` loads a dataset from flash into RAM, optionally selecting a specific dataset by its counter value.

`w` writes the current in-RAM records to flash as a new dataset using the circular storage mechanism.**

**Debugging commands:**

`e` erases the first page of the user flash region for maintenance or recovery purposes.

`s` scans the flash region for stored datasets and validates their presence.

`t` performs a synthetic flash write test to validate wrap-around and storage integrity.

`m` dumps the raw contents of a specified flash page by index for low-level debugging.

`x` resets the in-RAM record buffer without erasing flash data.

`f` modifies internal variables or flags via the serial interface.

`l` manually controls the RGB LED state.
`i` inserts a diagnostic power or information log record into the current dataset.
