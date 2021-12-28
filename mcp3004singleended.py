#!/usr/bin/env python3

import busio
import digitalio
import board
import adafruit_mcp3xxx.mcp3004 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn
import time

# create the spi bus
spi = busio.SPI(clock=board.SCK, MISO=board.MISO, MOSI=board.MOSI)

# create the cs (chip select)
cs = digitalio.DigitalInOut(board.D5)

# create the mcp object
mcp = MCP.MCP3004(spi, cs)

# create an analog input channel on pin 0
chan = AnalogIn(mcp, MCP.P2)

while True:
	value = chan.value
	voltage = chan.voltage
#	print('Raw ADC Value: ', value)
	print('ADC Voltage: ' + str(voltage) + 'V')
	time.sleep(0.001)
