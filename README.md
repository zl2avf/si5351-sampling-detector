# si5351-sampling-detector
Experimental 80MHz direct conversion receiver using an arduino controlled si5351 synthesiser 
and a modified Tayloe sampling detector.

This receiver has a constant -3dB passband of 6KHz at all frequencies and is set by the values of R5,C8,C9.

The formula I used to determine the passband was:    
XC8=NR5 
where XC8 is the reactance of C8 at half the passband (cutoff) frequency, 
N=number of capacitors (in this case 2),
and R5 is the series resistance feeding the switch

If you decide to change these values then change the value of C13 such that XC13=R7 at the same cutoff frequency.

The si5351 synthesiser has a maximum frequency of 160MHz which sets the top frequency for this receiver to 
80MHz. The PCB itself will go higher if you substitute an alternate signal source.

The Arduino *.ino code is slightly different from my other articles in that the I2C LCD displays the signal
frequency whereas the si5351 frequency is double the signal frequency. Step-sizes of 10,100,1000,10000,100000,
and 1000000Hz may be selected by short/long pushing the tuning knob on the rotary encoder.
