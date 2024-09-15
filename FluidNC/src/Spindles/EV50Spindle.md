# Basic protocol of EV50 spindle

All these things are tested

## EV50 General protocol

[id] [fcn code] [start addr]  [payload] [checksum]
        8bit        8bit      N*8bits    16bits

Adress coding
The EV50 spindles have 16bit and 32bit adress access methods, I'll only use the 16bit address method here.
As mentioned in the RS485 manual there are two different ways to change paramaters, first by changing them in EEPROM and second by changing them in RAM.
The manual mentiones that writing them in EEPROM may reduce the lifetime of the EEPROM.
Adresses are derived from the parameter numbers

Format  EEPROM Address      RAM Address
16bit   Parameter No. -1    Parameter No. -1 +32768

Follwing parameter are currently used by the EV50Spindle module
P02.91 control word
P02.90 set frequency
P10.21 running frequency

Afer that the adress must be converted to hexadecimal

Follwing function codes are allowed and used by the EV50
[03] read holding register
[06] preset single register 16bit
[10] preset multiple register 32bit


Atm I will only focus on three parameters Command codes, change frequency and running frequency
The command codes are parameter 2.91, so the adress is (291-1) [0122]
Parameter 2.91 is a command control word, as defined in following table from the manual

Command word(bit)   Definition
0                   Start
1                   Reverse
2                   Start reverse
3                   JOG
4                   Stop
5                   Emergency stop
6                   Safe stop
7                   Reset
9                   Parameter self-learning
10                  Tripping
11                  Pause
13                  UP (incremental)
14                  DOWN (decreasing)

The translation of the command word to hex is a bit cumbersome. If you want to start the spindle cw [start] you have to write the 1st bit of the control word 1.
In a 16bit message this means 0000000000000001 which equals to 0x0001
If you want to start ccw [start reverse] you have to write the 3rd bit of the controlword.
In a 16bit message this means 0000000000000100 which equals to 0x0004
To stop the spindle the 5th bit need to be set.
In a 16bit message this means 0000000000010000 which equals to 0x0010

So the full messages for start cw, start ccw and stop are as follwing
[slave address] [function code] [register address] [data] [crc checksum]
[01] [06] [01 22] [00 01] [e9 fc] -- forward start
[01] [06] [01 22] [00 04] [29 ff] -- reverse start
[01] [06] [01 22] [00 10] [29 f0] -- stop

To set the frequency we have to write to parameter P02.90. Parameter 02.90 is a procentual value of the range set by max frequency P02.18 and min frequency P02.19 with 3 decimal places.
Lets assume a standard 400Hz 24k RPM Spindle. Frequency range is set from 0 Hz to 400Hz. 
To set the spindle to run at 12.000 rpm we would have set the vfd to the frequency of 200Hz, which is half of the max value so 50%. With 3 decimal places the data needed to be send is 50000.
Which leads to following message
[01] [06] [01 20] [c3 50] [d9 30]


Read P02.19 (min frequency) and P02.18 (max frequency):

[01] [03] [00 da] [00 02] [e5 fo] min frequency 
[01] [03] [00 d9] [00 02] [15 fo] max frequency 

Tracking:

Parameter 10.21 [output frequency] which equals the hex address 03fc
function code 3
data  = N in N*8bits so in this case 2 for 16bit  

[01] [03] [03 fc] [00 02] [04 7f]

response should be


//[01] [04] [0000] [0002] -- output frequency
//gives [01] [04] [04] [00] [00] [0F] [crc16].

//01 04 |xx xx| |xx xx| |crc1 crc2|. So that's addr, cmd=04,
//2x 2xdata. First data seems running freq, second data set 
//freq. Running freq is *data[2,3]*.

//Set frequency:
//[01] [06] [0201] [07D0] Set frequency to [07D0] = 200.0 Hz. (2000 is written!)
