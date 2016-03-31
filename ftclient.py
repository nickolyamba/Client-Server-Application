#!/usr/bin/python

'''**************************************
# Name: Nikolay Goncharenko
# Email: goncharn@onid.oregonstate.edu
# Class: CS372-400
# Assignment: Project #2 ftclient.py
**************************************'''

'''***************************************************************************
# Program description: 	
#	Program is a client side of FTP-like application layer protocol. 
#  Valid Command Line Parameters: 
# 1. <server address> <server port> -l <data port>
# 2. <server address> <server port> -g <filename> <data port>
# After Control Connection is established, the program sends request to the server
# 	- invalid command is permitted to test server's  invalid command handling.
# 	-l <port> is a request for a list of files in the server's directory
#				Client receives  directory contents on a Data Connection initialted by 
#				server.  Server closes Data Connection.
#	-g <filename> <port> is a request for a particlar file from the server's directory
#				Client receives  file content on a Data Connection initialted by 
#				server and saves it in the client's  directory. Duplicates are appended with
#				number. Server closes Data Connection.
***************************************************************************'''

# Sources Used:
# https://docs.python.org/2/library/socket.html
# http://www.binarytides.com/python-socket-programming-tutorial/
# find ip of the client: ifconfig | awk '/inet addr/{print substr($2,6)}'
# ftclient.py flip2 38546 -g 38545 filename
# ftclient.py flip2 38546 -l 38545
#TESTS:
# ftclient.py flip 38546 -z 38540 README.txt
# ftclient.py flip 38546 -g README.tx 38540 
# ftclient.py flip 38546 -g shortfile.txt 38540 
# ftclient.py flip 38546 -l 38540

from socket import *
import select
import socket
import sys
import re
import os
from os import path

BUF_SIZE = 1024
SMALL_BUF_SIZE = 100
BACKLOG = 5

'''*****************************************************************************
* Function: get_args()
*
* Purpose:  This function gets command line arguments: server host name
*           
*
* Receives: None
*			 
*
* Returns:  SERVER_ADDR - server host name and port
*			SERVER_PORT - port number
*
* Pre:      None
*
* Post:     Server host name and port number are read
*****************************************************************************'''
def get_args():
	data_port = -1
	filename = '\0'

	if len(sys.argv) < 4:
		print "Error: Provide at least 3 command line arguments:\n\
chatclient.py  <SERVER_HOSTNAME> <SERVER_PORT>  <command>"
		sys.exit()
		
	if sys.argv[3] != "-l" and  sys.argv[3] != "-g":
		print "Client side validation! Error: 3d argument(command) must be either -l or -g"
		data_port = 35456
		#sys.exit()
		
	# if  command == "-g" get file
	if((sys.argv[3]) == "-g"):
		if len(sys.argv) < 6:
			print "Error: Provide at least 5 command line arguments to get file from the server:\n\
chatclient.py <SERVER_HOSTNAME> <SERVER_PORT>  -g <data_port><filename>"
			sys.exit()
		else:
			# 4th argement filename
			filename = sys.argv[4]
			# 5th data_port
			if ((sys.argv[5]).isdigit()):
				data_port = int(sys.argv[5])
				if data_port < 1025 or data_port > 65535:
					print "Error: data_port (argument #4) must be in the range [1025, 65535]"
					sys.exit()
			else:
				print "Error: data_port (argument #4) must be 16-bit number"
				sys.exit()
	
	# 1st argument - server_addr
	server_addr = sys.argv[1];
	# to facilitate input
	if  server_addr == "flip1":
		server_addr = "flip1.engr.oregonstate.edu"
	elif server_addr == "flip2":
		server_addr = "flip2.engr.oregonstate.edu"
	elif server_addr == "flip3":
		server_addr = "flip3.engr.oregonstate.edu"
	
	#  2nd argument - SERVER_PORT
	if ((sys.argv[2]).isdigit()):
		server_port = int(sys.argv[2]);
	else:
		print "Error: server_port (argument #2) must be 16-bit number"
		sys.exit()
	
	#  3d argument - command
	command = sys.argv[3]
	
	#  4th argument - data_port
	if((sys.argv[3]) == "-l"):
		if ((sys.argv[4]).isdigit()):
			data_port = int(sys.argv[4])
			if data_port < 1025 or data_port > 65535:
				print "Error: data_port (argument #4) must be in the range [1025, 65535]"
				sys.exit()
		else:
			print "Error: data_port (argument #4) must be 16-bit number"
			sys.exit()
	
	return server_addr, server_port, command, data_port, filename


'''***************************************************************************
* Function: initiate()
*
* Purpose:  This function resolves the server name to IP using 
*           gethostbyname(), then creates a socket and connects 
*           this socket to the server on provided IP and port
*
* Receives: 
*			SERVER_PORT: port number on the server received as a command line argument
*			SERVER_ADDR: human-readable address of the server received as a 
*				        command line argument
*
* Returns:  clientSocket - socket connected to the remote server
*
* Pre:      SERVER_PORT is a valid port
*			SERVER_ADDR is a valid host name
*
* Post:     None
***************************************************************************'''
def initiate(SERVER_PORT, SERVER_ADDR):
	try:
		remote_ip = socket.gethostbyname(SERVER_ADDR)
	except socket.gaierror: #could not resolve
		print 'Hostname could not be resolved. Exiting'
		sys.exit()
	
	clientSocket = socket.socket(AF_INET, SOCK_STREAM)
	clientSocket.connect((remote_ip, SERVER_PORT))
	print 'ControlSocket Connected to ' + SERVER_ADDR + ' on ' + remote_ip + ':' + str(SERVER_PORT) + "\n"
	
	return clientSocket


'''***************************************************************************
* Function: makeRequest()
*
* Purpose:  This function sends request to the server for establishing  
*                      a Data Connection
*
* Receives: 
*			clientSocket: socket of the client connected to server
*			request: string of parameters deliminated by "&" 
*
* Returns:  None
*
* Pre:      clientSocket is a valid socket connected to the server
*
* Post:     request message is sent to the server
***************************************************************************'''
def makeRequest(clientSocket, request):
	#print len(request)
	request = request.ljust(SMALL_BUF_SIZE, '\0')
	#print len(request)
	send_mess(clientSocket, request)


'''***************************************************************************
* Function: send_mess()
*
* Purpose:  This function sends message to the server via socket 
*           
*           
*
* Receives: 
*			clientSocket: socket of the client connected to server
*			prompt: client name + '>' + ' ' 
*			message: message to send to the server
*
* Returns:  None
*
* Pre:      clientSocket is a valid socket connected to the server
*			
*
* Post:     prompt+message are sent to the server
***************************************************************************'''
def send_mess(clientSocket, request):
	try:
		clientSocket.sendall(request)
	except socket.error:
		print 'send_mess() Failed'
		sys.exit()


'''***************************************************************************
* Function: is_valid_filename()
*
* Purpose:  Validate filename: Searh for any  FILENAME[0-9]* match
*						If found increment number at the end of filename
*						else returns as it is
*
*Sources used: http://goo.gl/Q7Ul4X 
* http://stackoverflow.com/questions/430079/how-to-split-strings-into-text-and-number
* http://stackoverflow.com/questions/6930982/variable-inside-python-regex
*              
* Receives: 
*			FILENAME
*
* Returns:  Filename to write in

*
* Pre:     None 
*			
*
* Post:     the filename validated
***************************************************************************'''	
def is_valid_filename(FILENAME):
	
	append = ''
	max_append_idx  = -100
	isInitialFileName = 0
	
	# get list of files on the current directory
	files = [f for f in os.listdir('.') if path.isfile(f)]
	for file in files:
		# find the macth FILENAME[0-9]+ case sensitive
		match = re.match(FILENAME + r"([0-9]*)", file, re.I)
		# change 
		if match:
			items = match.groups()
			if len(items) == 1:
				# if items[0] is not None
				if items[0]:
					append = int(items[0])
					append += 1
					# choose max end index for a filename
					if max_append_idx < append:
						max_append_idx = append
				else:
					#FILENAME += str("1")
					max_append_idx = 1
					
	if max_append_idx > 0:
		print "File '" + FILENAME + "' already exists, and it's saved as " + FILENAME + str(max_append_idx)
		return FILENAME + str(max_append_idx)
	else:
		return FILENAME
	
	
'''***************************************************************************
* Function: fill_buffer()
*
* Purpose:  This function reads a message from the server via Data Connection socket 
*						 It continues to read until the entire length of packet (BUF_SIZE) is read  
* 
* Receives: 
*			dataSocket: Data Connection socket of the client connected to a server
*
* Returns:  ''.join(chunks)  contains string of data read
*						bytes_read
*						0 - if server closed it's socket on Data Connection, all the data is read
*
* Pre:      dataSocket is a valid socket connected to the server
*			
*
* Post:     the entire packet is read and data is provided to receiveFile() function
***************************************************************************'''	
def fill_buffer(dataSocket):
	chunks = []
	#chunks = ''
	bytes_recd = 0
	
	# read while bytes recd < BUF_SIZE
	while bytes_recd < BUF_SIZE:
		chunk = dataSocket.recv(BUF_SIZE)
		
		# Nothing to read, server's side socket closes
		if not chunk:
			return '', 0
		
		chunks.append(chunk.rstrip('\0'))
		bytes_recd = bytes_recd + len(chunk)
		#print  "chunk length: " + str(len(chunk))
		#print "chunk: " + chunk
		
	return ''.join(chunks), bytes_recd # joins list of strings into string
	
	
'''***************************************************************************
* Function: receiveFile()
*
* Purpose:  This function reads a message from the server via socket 
*           and then prints it
*           https://docs.python.org/2/tutorial/inputoutput.html
*
* Receives: 
*			dataSocket: socket of the client connected to a server
*
* Returns:  True if server replied with '\quit'; otherwise, returns false
*
* Pre:      dataSocket is a valid socket connected to the server
*			
*
* Post:     replied message is read
***************************************************************************'''
def receiveFile(dataSocket, FILENAME):
	initial_name =  FILENAME
	file_size = 0
	#bytes_recd = 1
	packet = ''
	
	FILENAME = is_valid_filename(FILENAME)
	
	file_obj = open(FILENAME, "w")
	(packet, bytes_recd) = fill_buffer(dataSocket)
	
	while bytes_recd > 0:
		#packet.rstrip('')
		file_obj.write(packet)
		(packet, bytes_recd) = fill_buffer(dataSocket)
		#print  "packet length: " + str(len(packet))
		#print "packet: " + packet

	print "File '" + initial_name + "' transfer is complete\n"
	file_obj.close()
	

'''***************************************************************************
* Function: receive_all()
* Sources used:  https://docs.python.org/2/howto/sockets.html
*
* Purpose:  This function reads from Data Connection socket a fixed lentgh message
* 			by established  by me protocol, where server and client send messages
*			each other of a fixed length = BUF_SIZE. If message smaller, it's appended
*			with '\0'
*           
*
* Receives: 
*			dataSocket: socket of the Data Connection
*
* Returns:  string containing all the data recieved of length = BUF_SIZE;
*
* Pre:      dataSocket is a valid socket connected to the server
*			
*
* Post:     message from Data Connection is read
***************************************************************************'''
def receive_all(dataSocket):
	chunks = []
	bytes_recd = 0
	
	# read until bytes recd = BUF_SIZE
	while bytes_recd < BUF_SIZE:
		chunk = dataSocket.recv(BUF_SIZE*2)
		#print  "chunk length: " + str(len(chunk))
		#print "chunk: " + chunk
		
		# Nothing to read, server's side socket closes
		if not chunk:
			break

		chunks.append(chunk)
		bytes_recd = bytes_recd + len(chunk)

	return ''.join(chunks), bytes_recd	# joins list of strings into string
	

	'''***************************************************************************
* Function: read_controlSocket()
*
* Purpose:  This function reads a message from the server via socket 
*           and then prints it
*           
*
* Receives: 
*			dataSocket: socket of the client connected to a server
*
* Returns:  True if server replied with '\quit'; otherwise, returns false
*
* Pre:      dataSocket is a valid socket connected to the server
*			
*
* Post:     replied message is read
***************************************************************************'''
def read_controlSocket(socket):
	(reply, bytes_recd) = fill_buffer(socket) 
	print reply


def main():
	SERVER_ADDR, SERVER_PORT, COMMAND, DATA_PORT, FILENAME = get_args()
	#print "Args: " + SERVER_ADDR + " "+ str(SERVER_PORT)+ " "+COMMAND+ " "+str(DATA_PORT)+ " "+FILENAME
	 # Symbolic name meaning all available interfaces
	HOST = ''
	global data_socket
	request = COMMAND + "&" + str(DATA_PORT) + "&" + FILENAME
	
	servSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	print "\nServSocket for receiving connection is created" 
     
	try:
		servSocket.bind((HOST, DATA_PORT))
	except socket.error , msg:
		print 'Bind failed. Error Code : ' + str(msg[0]) + '  ' + msg[1]
		sys.exit()
	 
	print "ServSocket bound to port " + str(DATA_PORT)

	servSocket.listen(BACKLOG)
	print 'servSocket is listening now...'
	
	# create a socket and bind it to the server/port address
	clientSocket = initiate(SERVER_PORT, SERVER_ADDR)
	makeRequest(clientSocket, request)
	
	# list of input sockets
	input = [clientSocket, servSocket]
	
	run = 1
	while run: 
		input_ready,output_ready,except_ready = select.select(input,[],[])
		# traverse sockets and respond when there is an action
		for sock in input_ready:
			
			if sock == servSocket:
				data_socket, data_addr = servSocket.accept()
				if COMMAND == '-g':
					print "Receiving '" + FILENAME + "' from " + data_addr[0] + ':' + str(data_addr[1]) + '...'
					receiveFile(data_socket, FILENAME)
				elif COMMAND == '-l':
					print "Receiving directory structure from " + data_addr[0] + ':' + str(data_addr[1]) + '...'
					read_controlSocket(data_socket)
				data_socket.close()
				run = 0
			
			elif sock == clientSocket:
				print  SERVER_ADDR + ':' + str(SERVER_PORT) + '  says:'
				read_controlSocket(clientSocket)
				#clientSocket.close()
				run = 0
	
	clientSocket.close()
	servSocket.close()

if __name__ == "__main__":
   main()


