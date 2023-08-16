#!/usr/bin/env python2

import serial

# start with python2 BusPirate_I2C_sniffer.py
#
# created from https://swharden.com/blog/2017-02-04-logging-i2c-data-with-bus-pirate-and-python/

BUSPIRATE_PORT = '/dev/ttyUSB0' #Bus pirate port
DEVICE_ADDRESS = '0x23' #Renault scenic 2 dash address

def send(ser, cmd, silent=False):
    """
    send the command and listen to the response.
    returns a list of the returned lines.
    The first item is always the command sent.
    """
    ser.write(str(cmd+'\n').encode('ascii')) # send our command
    lines=[]
    for line in ser.readlines(): # while there's a response
        lines.append(line.decode('utf-8').strip())
    if not silent:
        print("\n".join(lines))
        print('-'*60)
    return lines
    
def decode(raw):
    """
    decode the line
    a single line can contains multiple packets
    """
    raw = raw.replace("][", "]\n[")
    output = "{} {}({})[{}] DATA:{}"
    
    for line in raw.splitlines():
		
		datalenght = (len(line)-7)/5 #remove 2 for "[]", remove 5 for address, divide by 5 for byte + type
		
		if datalenght < 0:
			return
		
		address = line[1 : 6]
		packet = []
		for x in range(1, datalenght + 1):
			value = line[x * 5 +1 : x * 5 + 5 +1]
			nack = ""
			if value.endswith('-'):
				nack = "NACK"
			
			packet.append(value[0:4] + nack)
		packetString = ",".join(packet)
		
		if line.startswith('[0x46'): # write
			print(output.format(line, "WRITE", datalenght, address, packetString))
		else:
			if line.startswith('[0x47'): # read
				print(output.format(line, "READ", datalenght, address, packetString))
			else:
				print(line)

# the speed of sequential commands is determined by this timeout
ser=serial.Serial(BUSPIRATE_PORT, 115200, timeout=.1)

# have a clean starting point
send(ser,'#',silent=True) # reset bus pirate (slow, maybe not needed)

# set mode to I2C (doc http://dangerousprototypes.com/blog/bus-pirate-manual/i2c-guide/)
send(ser,'m',silent=True) # change mode (goal is to get away from HiZ)
send(ser,'4',silent=True) # mode 4 is I2C
send(ser,'3',silent=True) # 100KHz
send(ser,'(2)',silent=True) # Sniffer mode

data=[]

#try:
print("reading data until CTRL+C is pressed...")
while True:
	for line in ser.readlines(): # while there's a response
		line = line.decode('utf-8').strip()
		#decode(line)
		data.append(line)
		print(line)
    
#except:
#    print("exception broke continuous reading.")
#    print("read %d data"%len(data))

send(ser,'A',silent=True) # Exit sniffer mode
ser.close() # disconnect so we can access it from another app

print("disconnected!") # let the user know we're done.
