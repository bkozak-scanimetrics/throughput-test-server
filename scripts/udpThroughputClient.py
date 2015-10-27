#!/usr/bin/env python
"""Sends a bunch of packets to a UDP server

Sends some packets to an IPV6 UDP server. Includes a special stop sequence
in its final packet so the server can tell when the connection should be closed.
"""

import socket
import sys

MESSAGE_COUNT = 1024
MESSAGE_LEN=1024

#number of times to repeat the stop sequence
STOP_REPEAT=8

def sendOrDie(sock,msg,addr):
	"""Send to socket or exit program
	"""

	try:
		sock.sendto(msg, addr)
	except Exception as e:
		sys.stderr.write("Error: unable to communicate on socket\n")
		sys.stderr.write(str(e)+'\n')
		exit(-1)


if __name__ == '__main__':
	"""Program entry point
	"""
	if(len(sys.argv) != 3):
		sys.stderr.write("Error: expected 2 arguments. Need ip addr and port")
		exit(-1)


	addr = sys.argv[1]

	try:
		port = int(sys.argv[2])
	except ValueError as e:
		sys.stderr.write("Error: port must be an integer!")
		exit(-1)


	try:
		sock = socket.socket(socket.AF_INET6,socket.SOCK_DGRAM)
	except:
		sys.stderr.write("Error: unable to create socket!")
		exit(-1)

	sent = 0
	msg = ''.join([chr(0) for i in xrange(MESSAGE_LEN)])
	stopMsg = ''.join([chr(0xFF) for i in xrange(MESSAGE_LEN)])

	for i in xrange(MESSAGE_COUNT-1):
		sendOrDie(sock,msg,(addr,port))

	#send the stop sequence a few times to give a high chance that the server
	#actually recieved it
	for i in xrange(STOP_REPEAT):
		sendOrDie(sock,stopMsg,(addr,port))
