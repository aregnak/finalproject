#!bin/sh

if (avra -I ./include/  main.asm); then
    sudo avrdude -c avrispmkII -p Atmega8515 -P usb -U flash:w:main.hex
else
    echo "Error compiling. Check error message above"
fi
