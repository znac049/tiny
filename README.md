# Notes:

We'll run the part at 16MHz

 Run the part at 8MHz
 You need to reprogram the fuses. In the following example, I use the USBTiny programmer. You will of course
 need to replace that with whatever programmer you have (the -c option)


 ## Fuses
 Read existing fuses:
 avrdude -c usbtiny -p t85 -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h

 As standard, I reccommend the following fuse combination, which will clock the part at 16MHz
 -U lfuse:w:0xF1:m -U hfuse:w:0xDF:m -U efuse:w:0xFE:m

 However I found it useful, while debugging to enable CKOUT on pin PB4, in which case use
 -U lfuse:w:0xB1:m -U hfuse:w:0xDF:m -U efuse:w:0xFE:m