EIA RS-170A Analog Video RCA Documentation.

Electrical specifications.
75 Ohm load.
IRE unit (relative to blanking),
1IRE = 1.0V/140

Sync tip -40 IRE | -0.285714 V
Blanking level 0 IRE | 0V
Black level +7.5 IRE | +0.053571 V
White level +100 IRE | +0.714286 V
Colorburst amplitude +-20 IRE +- 0.142857 V

Signal timings

Horizontal line 
63.5555 ųs

Horizontal Sync pulse width
4.7 with Error marging of 0.1 ųs 
Blanking level 1.5 +- with error marging of 0.1ųs

Breezeway 0.6ųs blanking level btween H Sync and burst start

Colorburst 9 +- 1 cycles of 3.579545 MHZbfor 2.51ųs at 180° phase

Back porch 4.7ųs H Sync end and start of video

Video window
52.65ųs
Formula for active pixel:
E(t) = Y + U * sin(2pi * Fsubcarrier) + Vcos(2pi * Fsubcarrier)

Where Fsubcarrier= (MHZ)= 315/88 
or ~3.579545

Where Fhorizontal = 4.5Mhz/286
Or 15734.264 HZ
Hlength = 1/Fhorizontal 
Or ~63.55554 ųs