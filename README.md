<p align="center">
  <b >Two connection client-server network application</b>
</p>
The application allows a client to list and download \*.txt files from a server directory
####Usage:</br>
1) Compilation:  type `> make` on a command line to compile 'ftserver.c' file
	   
2) Run ftserver: `> ftserver <server port>`

3) Run ftclient.py:</br>
* to get list of files to download, run:</br>
	`> ftclient.py <server address> <server port> -l <data port>`
* to download a selected file, run:  
	`> ftclient.py <server address> <server port> -g <filename> <data port>`

---
####Examples:

1) List Files on the server</br>
Server:
```
> ftserver 38546
Server open on 38546
Started listening for a new client to connect...
```
Client:
```
> ftclient.py flip 38546 -l 38540
ftserver.o
makefile
README.txt
ftserver.c
ftserver
shortfile.txt
```

Server:
```
Connection from: 128.193.54.7
List directory requested on port 38540
Sending directory contents to 128.193.54.7:38540
```
</br>
2) Get longfile.txt (~40Mb)</br>
Client:
```
> ftclient.py flip 38546 -g shortfile.txt 38540 
ServSocket for receiving connection is created
ServSocket bound to port 38540
servSocket is listening now...
ControlSocket Connected to flip on 128.193.54.7:38546
Receiving 'shortfile.txt' from 128.193.54.7:40669...
File 'shortfile.txt' transfer is complete
```

Server:
```
Connection from: 128.193.54.7
File "shortfile.txt" requested on port 38540
```
</br>
3) Handling of an Invalid Command</br>
Client:
```
> ftclient.py flip 38546 -z 38540 README.txt
Client side validation! Error: 3d argument(command) must be either -l or -g
ServSocket for receiving connection is created
ServSocket bound to port 35456
servSocket is listening now...
ControlSocket Connected to flip on 128.193.54.7:38546
flip:38546  says:
ERROR: 3d argument (COMMAND) must be either -l , or -g!
```
Server:
```
Connection from: 128.193.54.7
Invalid command by client. Sending ERROR message to 128.193.54.7
```
</br>
4) Handling of Invalid filename</br>
Client:
```
> ftclient.py flip 38546 -g shortfile.tx 38540 
ServSocket for receiving connection is created
ServSocket bound to port 38540
servSocket is listening now...
ControlSocket Connected to flip on 128.193.54.7:38546
flip:38546  says:
ERROR: FILE NOT FOUND!
```

Server:
```
Connection from: 128.193.54.7
File "shortfile.tx" requested on port 38540
File shortfile.tx not found. Sending error message to 128.193.54.7:38540!
```
</br>
5) Downloading duplicate from the server:
Client:
```
> ftclient.py flip 38546 -g shortfile.txt 38540
ServSocket for receiving connection is created
ServSocket bound to port 38540
servSocket is listening now...
ControlSocket Connected to flip on 128.193.54.7:38546
Receiving 'shortfile.txt' from 128.193.54.7:40684...
File 'shortfile.txt' already exists, and it's saved as shortfile.txt1
File 'shortfile.txt' transfer is complete
```

Server:
```
Connection from: 128.193.54.7
File "shortfile.txt" requested on port 38540
```

