# YUV scaling
4:2:2 ->
4:0:0 -> black and white, 1 byte per pixel
evenX = x & ~1;
That snaps x down to the nearest even number.
So x = 0 -> x = 0
So x = 1 -> x = 0
So x = 2 -> x = 2
So x = 3 -> x = 2
So x = 4 -> x = 4
So x = 5 -> x = 4

## Block index
= (evenX + sizeX * Y) * 2
That maps (x,y) to the one dimentional array
In steps of 4 bytes

Then to write one pixel

Where (x,y, Y,U,V)
Maps to
Buffer[Block index] = Y
Buffer[Block index + 1] = U
Buffer[Block index + 2] = V

(If odd, Y index + 1)
 
Then to read
For i + 4
Buffer[i]
Y0 = I
Y1 = i + 1
U = I + 2
v = i + 3

// Replace old LUT lookup inside the line generation loop
uint8_t dac0 = clamp_dac(y + v);
uint8_t dac1 = clamp_dac(y + u);
uint8_t dac2 = clamp_dac(y - v);
uint8_t dac3 = clamp_dac(y - u);

line_buffer[x] = (dac3 << 24) | (dac2 << 16) | (dac1 << 8) | dac0;