# Anti Em Help

## Compatible Pocket Computers

Sharp EL-5500, EL-5500II, PC-1401, PC-1402

## Basic Usage

Anti Em's interface to the Sharp Pocket Computer is a printer emulator, so commands are issued to Anti Em as though they were being printed. There are two ways to do this:

* Direct:  Enable printing with _Shift-Enter_ and run commands in RUN mode. If the command contains any non-numeric characters, it must be enclosed in quotation marks.
* Program: Send commands with either _LPRINT_ or by using _PRINT = LPRINT_ in a program
    
To change Anti Em's mode, print a string to Anti Em containing @ and the mode name. The Anti Em modes are:

* **@EM** _(printer emulator)_
* **@CV** _(send 0-5V control voltage and gate (high/low)_
* **@MIDI** _(send MIDI commands)_
* **@HELP** _(return to @EM mode, and write a help screen to the terminal)_

## @EM Usage

Print to Anti Em while its USB port is connected to a computer. Use a terminal program to listen to the USB port at 9600 baud.

## @CV Usage

### D{0-4095}

Emits voltage corresponding to the DAC value. This is uncalibrated voltage, and can be used to calibrate Anti Em

### ={0-4095}

Saves the specified DAC value to EEPROM, and treats it as the value at 1V. Use the D command and a volt meter to find the DAC value that sends 1V, and then use = with that number to calibrate Anti Em. You may return Anti Em to its default calibration by printing the **=0** command

### {0.00-5.00}

Print a floating point number from 0 to 5 to emit the specified voltage. The maximum voltage depends on the Arduino's max output. If the voltage emitted does not match the expression, then follow the calibration procedure described under the **=** command

### +

Gate on. The gate output goes high, to the Arduino's max output

### -

Gate off. The gate output goes low, to 0V
    
## @MIDI Usage

### N{0-127}

Note On. Sends a note message for the current channel, at the current velocity. If a note was previously on, send a note off first

### X

Note Off. Sends a note off message for the last note on, on the last channel

### C{1-16}

Set MIDI channel for subsequent note on messages. Defaults to 1 on power-up.

### V{0-127}

Set MIDI velocity for subsequent note on messages. Defaults to 127 on power-up. _NOTE_: At velocity of 0, Anti Em will send note off messages for the **N** command

