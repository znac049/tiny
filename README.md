
 - run the part at 8MHz
 - use low power mode

 Run the part at 8MHz
 You need to reprogram the fuses. In the following example, I use the USBTiny programmer. You will of course
 need to replace that with whatever programmer you have (the -c option)

 Read existing fuses:
 avrdude -c usbtiny -p t85 -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h