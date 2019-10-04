/*
 * Copyright (c) 2019, Jason Justian, Beige Maze
 * 
 * Portions (c) 2016, Walter Fischer, used with permission
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
  
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <EEPROM.h>

/* This is the starting point for DAC steps-per-volt calibration. It's experimentally-determined
 *  with my Nano and my meter, but I've noticed that it varies according to power supply. So
 *  in real life, you'll want to calibrate your AntiEm in @CV mode with the D and = commands.
 */
const int DEFAULT_VOLT_REF = 876; // Calibration of DAC at 1V

const int OUT_ACK = 4; // Pin D4, ACK Sharp CE-126P, pin 9 of the 11-pin. socket strip
const int IN_Xout = 6; // Pin D6, Xout Sharp PC-1401, pin 7 of the 11-pin. socket strip
const int IN_Dout = 8; // Pin D8, Dout Sharp PC-1401, pin 5 of the 11-pin. socket strip
const int IN_Busy = 9; // Pin D9, Busy Sharp PC-1401, pin 4 of the 11-pin. socket strip
const int CV_Gate = 3; // Pin D3, CV gate on/off
const int InfoLED = 13; // The internal LED is lit when data is received from the Sharp
const int MIDILED = 12; // Lit when the @MIDI mode is active
const int CVLED = 11; // Lit when the @CV mode is active

const int MODE_EM = 1; // Printer emulator (@EM)
const int MODE_MIDI = 2; // MIDI interface (@MIDI)
const int MODE_CV = 4; // CV generator (@CV)

Adafruit_MCP4725 dac;
boolean busy; // Internal state flag
boolean Xout; // Internal flag for the state of pin 7
int d_bit; // Internal state of the last bit received
int d_byte; // Internal register for data byte construction
long timeout;
int i; // Internal counter for bit index
int mode; // Selected mode (see above for available modes)
String buff; // Command buffer

int volt_ref; // Default calibration
int midi_channel;
int midi_note;
int midi_velocity;
int midi_last_channel;

void setup () {
    Serial.begin(9600);
    pinMode(OUT_ACK, OUTPUT);
    pinMode(IN_Xout, INPUT);
    pinMode(IN_Dout, INPUT);
    pinMode(IN_Busy, INPUT);
    pinMode(CV_Gate, OUTPUT);
    pinMode(InfoLED, OUTPUT);
    pinMode(MIDILED, OUTPUT);
    pinMode(CVLED, OUTPUT);
    mode = MODE_EM;

    // MIDI Setup
    midi_channel = 0;
    midi_last_channel = 0;
    midi_note = -1;
    midi_velocity = 127;

    // CV Setup
    dac.begin(0x60);
    int msb = EEPROM.read(0);
    int lsb = EEPROM.read(1);
    volt_ref = (msb * 256) + lsb;
    if (volt_ref > 1024 || volt_ref < 256) volt_ref = DEFAULT_VOLT_REF;

    // Introduction Screen
    Serial.print("**** Beige Maze AntiEm v1.0 ****\n");
    Serial.print("(c) 2019, Jason Justian\n");
    Serial.print("www.beigemaze.com\n\n");
    Serial.print("CV 1V DAC=");
    Serial.print(volt_ref);
    Serial.print("\n\nREADY.\n");
}

void loop () {
    digitalWrite(InfoLED, LOW);
    Xout = digitalRead(IN_Xout);

    if (Xout) {
        // DeviceSelect:
        digitalWrite(InfoLED, HIGH);
        delayMicroseconds(50);
        i = 0;
        do {
            digitalWrite(OUT_ACK, HIGH);
            timeout = millis();
            do {
                busy = digitalRead(IN_Busy);
                if (millis() - timeout > 50) break;
            } while (!busy);
            delayMicroseconds(50);
            digitalWrite(OUT_ACK, LOW);
            do {
                busy = digitalRead(IN_Busy);
            } while (busy);
            delayMicroseconds(150);
            i++;
        } while (i < 8);
    }

    busy = digitalRead(IN_Busy);

    if (busy && !Xout) {
        digitalWrite(InfoLED, HIGH);
        i = 0;
        d_byte = 0;
        do {
            do {
                busy = digitalRead(IN_Busy);
            } while (!busy);
            delayMicroseconds(50);
            d_bit = digitalRead(IN_Dout);
            digitalWrite(OUT_ACK, HIGH);
            do {
                busy = digitalRead(IN_Busy);
            } while (busy);
            delayMicroseconds(50);
            digitalWrite (OUT_ACK, LOW);
            d_byte = d_byte | (d_bit << i);
            i++;
        } while (i < 8);

        if (d_byte == 13) {
            // Is this a mode change?
            if (buff.startsWith("@EM")) {
                mode = MODE_EM;
                Serial.begin(9600);
                digitalWrite(MIDILED, LOW);
                digitalWrite(CVLED, LOW);
            } else if (buff.startsWith("@CV")) {
                mode = MODE_CV;
                Serial.begin(9600);
                digitalWrite(MIDILED, LOW);
                digitalWrite(CVLED, HIGH);
            } else if (buff.startsWith("@MIDI")) {
                mode = MODE_MIDI;
                Serial.begin(31250);
                digitalWrite(MIDILED, HIGH);
                digitalWrite(CVLED, LOW);
            } else {
                // If this isn't a mode change, handle the end of line as a command, or
                // send a character to the terminal.
                if (mode == MODE_EM) Serial.println();
                if (mode == MODE_CV) handleCVCommand(buff);
                if (mode == MODE_MIDI) handleMIDICommand(buff);
            }
            buff = "";
        } else {
            if (d_byte == 48) d_byte = 'O';
            if (d_byte == 240) d_byte = '0';
            if (d_byte > 31 && d_byte < 127) {
                buff += char(d_byte);
                if (mode == MODE_EM) Serial.print(char(d_byte));
            }
        }
    }
}

void handleCVCommand(String cmd) {
    String op = cmd.substring(0, 1);
    if (op == "+") {
        // Gate on
        digitalWrite(CV_Gate, HIGH);
    } else if (op == "-") {
        // Gate off
        digitalWrite(CV_Gate, LOW);
    } else if (op == "=") {
        // Set calibration voltage
        String new_1v = cmd.substring(1);
        volt_ref = new_1v.toInt();
        volt_ref = constrain(volt_ref, 0, 4095);
        int msb = volt_ref / 256;
        int lsb = volt_ref - (msb * 256);
        EEPROM.update(0, msb);
        EEPROM.update(1, lsb);
    } else if (op == "D") {
        // DAC value out
        String new_dacv = cmd.substring(1);
        int val = new_dacv.toInt();
        val = constrain(val, 0, 4095);
        dac.setVoltage(val, false);
    } else {
        // Voltage out
        float v = cmd.toFloat();
        int val = v * volt_ref;
        val = constrain(val, 0, 4095);
        dac.setVoltage(val, false);
    }
}

void handleMIDICommand(String cmd) {
    String op = cmd.substring(0, 1);
    if (op == "C") {
        digitalWrite(MIDILED, LOW);
        // Set MIDI channel
        String new_channel = cmd.substring(1);
        midi_channel = new_channel.toInt() - 1;
        midi_channel = constrain(midi_channel, 0, 15);
    } else if (op == "V") {
        // Set MIDI velocity
        String new_velocity = cmd.substring(1);   
        midi_velocity = new_velocity.toInt();
        midi_velocity = constrain(midi_velocity, 0, 127);
    } else if (op == "X") {
        // Note off
        if (midi_note > -1) {
            MIDIOut(midi_last_channel, midi_note, 0);
            midi_note = -1;
        }
    } else {
        // Note on
        int note_number = cmd.toInt();
        note_number = constrain(note_number, 0, 127);
        if (midi_note > -1) {
            // Turn previous note off
            MIDIOut(midi_last_channel, midi_note, 0);
        }
        midi_note = note_number;
        midi_last_channel = midi_channel;
        MIDIOut(midi_channel, midi_note, midi_velocity);
    }
}

void MIDIOut(int ch, int note, int vel) {
    char status_msg = vel ? 0x90 : 0x80; /* Velocity = 0 sends Note Off */
    status_msg += ch; /* Add low nybble channel number */
    Serial.write(status_msg);
    Serial.write(note);
    Serial.write(vel);
}