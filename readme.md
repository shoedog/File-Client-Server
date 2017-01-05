Server can send any file type. Tested on sizes up to 1 GB.
Client can receive these files successfully.

Python Client

Start: ```python ftclient.py <SERVER> <SERVER_PORT> <DATA_PORT> -c=<COMMAND> -f=<FILENAME>```

Example ```python ftclient.py flip1 4000 4500```
Command and filename are optional and are handled in the program if not entered on the command line

Execution & Control:
- Input command at prompt: will only accept -g or -l
- Input filename at prompt if option was -g
- You can also input -g <FILENAME> at the command prompt
- Client will validate command line and input arguments, connect to server, and send command

**Tested on OSU flip servers**


C Server - make is tested on Linux only

build: ```make all```
run: ```./ftserver <PORT to Listen On>```
clean: ```make clean```

Execution & Control:
- Server will print status messages
- Once client session ends, server is available for another session
- CTRL-C to exit

**Tested on OSU flip servers**

