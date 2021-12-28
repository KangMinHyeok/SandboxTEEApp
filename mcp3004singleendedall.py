#!/usr/bin/env python3

import busio
import digitalio
import board
import adafruit_mcp3xxx.mcp3004 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn

# create the spi bus
spi = busio.SPI(clock=board.SCK, MISO=board.MISO, MOSI=board.MOSI)

# create the cs (chip select)
cs = digitalio.DigitalInOut(board.CE0)

# create the mcp object
mcp = MCP.MCP3004(spi, cs)

# create an analog input channel on pin 0
chans = [AnalogIn(mcp, pin) for pin in [MCP.P0, MCP.P1, MCP.P2, MCP.P3]]

for chan in chans:
	print("channel:", chan)
	print('Raw ADC Value: ', chan.value)
	print('ADC Voltage: ' + str(chan.voltage) + 'V')
