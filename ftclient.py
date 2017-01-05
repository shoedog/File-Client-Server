"""
filename: ftclient.py
author: Wesley Jinks
date: 11/29/2016

Sources used:
    https://www.tutorialspoint.com/python/os_listdir.htm
    https://www.tutorialspoint.com/python/python_networking.html

    http://www.binarytides.com/receive-full-data-with-the-recv-socket-function-in-python/
    https://pymotw.com/2/select/
    http://stackoverflow.com/questions/27241804/sending-a-file-over-tcp-sockets-in-python
    http://ilab.cs.byu.edu/python/select/echoserver.html

    Python Docs: https://docs.python.org/2/library/socket.html
                https://docs.python.org/2/library/select.html
                https://docs.python.org/2.7/tutorial/errors.html
                https://docs.python.org/2/library/argparse.html
"""


import sys, os
import socket
import argparse
import select
from types import *
from time import sleep


def return_args():
    # Setup parser for command line args
    parser = argparse.ArgumentParser(prog='ftclient')
    parser.add_argument('server', type=str, help='FileServer IP Address')
    parser.add_argument('server_port', type=int, help='FileServer Port')
    parser.add_argument('-c', '--command', choices=['-l', '-g'], help='FileServer CMD: -c-l (list files) -c-g <FILENAME> (get file)')
    parser.add_argument('-f', '--filename', type=str, help='filename to get from the server')
    parser.add_argument('data_port', type=int, help='data port to setup a TCP data connection on')
    args = parser.parse_args()
    server = args.server
    server_port = args.server_port
    command = args.command
    filename = args.filename
    data_port = args.data_port

    # Validate Server Port
    if server_port < 1024:
        print ("Invalid Server Port Number: %d, valid Port Range: 1024-49151", server_port)
        sys.exit(2)
    elif server_port > 49151:
        print ("Invalid Server Port Number: %d, valid Port Range: 1024-49151", server_port)
        sys.exit(2)

    # Validate server address
    try:
        assert (server == "flip1" or server == "flip2" or server == "flip3")
    except AssertionError, server:
        print ("Invalid Server: %s" %server)
        sys.exit(2)
    else:
        return server, server_port, command, filename, data_port


def cmd_handler(cmd, file_name):
    """

    :param cmd: if none or not (-g or -l) will prompt for -g or -l,
    :param file_name: if none will prompt for filename
    :return: validated command and validated filename
    """
    filename = file_name
    if cmd is None:
        cmd = raw_input("Enter -l to list files or -g <FILENAME> to get a file: ")

    while ("-l" not in cmd) and ("-g" not in cmd):
        cmd = raw_input("Valid commands are (-l or -g FILENAME) -l to list files or -g FILENAME to get a file: ")

    if cmd == '-g':
        filename = raw_input("You must enter a filename for -g: ")
    elif "-g" in cmd:
        g = cmd.split(' ')
        cmd = g[0]
        filename = g[1]

        if cmd != '-g':
            cmd, filename = filename, cmd

    if filename is not None:
        cmd = cmd + " " + filename

    return cmd, filename


def get_data_port(port):
    """

    :param port: data port number
    :return: validated port number
    """
    while port is None:
        p = raw_input("Enter the port number to establish a data connection: ")
        port = int(p)

    while (port < 1024) or (port > 49151):
        print ("Invalid Data Port: %d, valid Port Range: 1024-49151", port)
        p = raw_input("Enter a valid port number to establish a data connection: ")
        port = int(p)

    return port


# Get local host name and ip-address
def get_machine_info():
    """Prints IP Address and Host Name to Console

    :return: IP Address

    Purpose: To let the user know the IP Address and Host for creating connections
        where the IP address and host name may vary
    """
    host_name = socket.gethostname()
    ip_address = socket.gethostbyname(host_name)
    # DEV NOTE: flip3.engr.oregonstate.edu = 128.193.36.41
    return ip_address


def main():
    # get data from command line parser
    server, server_port, command, filename, data_port = return_args()
    server += ".engr.oregonstate.edu"

    # initialize command socket and connect
    p = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    p.connect((server, server_port))

    # validate and get command and data_port
    command, filename = cmd_handler(command, filename)
    data_port = get_data_port(data_port)

    # bind and open data socket for listening
    q = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    q.bind((socket.gethostbyname(socket.gethostname()), data_port))

    # initialize input sources for select.select
    input_sources = [p, q]

    # send data_port and command
    p.send(str(data_port))
    sleep(1)
    p.send(command)

    # listen on data port
    q.listen(2)

    # select.select loop to handle command and data sockets
    running = True
    while running:
        readable, writeable, exceptready = select.select(input_sources, [], [], 0)

        # loop over readable sockets
        for s in readable:

            # command connection
            if s == p:
                error = s.recv(1024)
                if error:
                    print ("%s" % error )
                    running = False
                    break

            # accept data connection
            elif s == q:
                server, address = q.accept()
                input_sources.append(server)

            # data connection is open, handle -l or -g data from server
            else:
                if command == '-l':
                    host, port = socket.getnameinfo(s.getpeername(), socket.NI_NUMERICSERV)
                    client, d_port = socket.getnameinfo(s.getsockname(), socket.NI_NUMERICSERV)

                    print ("\nReceiving directory structure from %s:%s" % (host, d_port))

                    while True:
                        data = s.recv(1024)
                        if data:
                            data = data.rstrip('\n')
                            print >> sys.stderr, "\n%s" % (data)
                            break
                else:
                    path = "."
                    dirs = os.listdir( path )
                    for file in dirs:
                        if file == filename:
                            dec = raw_input("File already exists overwrite(y/n): ")
                    if dec != "y":
                        print ("Not overwriting file")
                        running = False
                        break
                    else:
                        print ("Overwriting file")
                        f = open(filename, 'wb')
                        host, port = socket.getnameinfo(s.getpeername(), socket.NI_NUMERICSERV)
                        client, d_port = socket.getnameinfo(s.getsockname(), socket.NI_NUMERICSERV)
                        print ('Receiving "%s" from %s:%s' %(filename, host, d_port))
                        while True:
                            data = s.recv(1024)
                            if data:
                                f.write(data)
                            if not data:
                                break

                        f.close()
                        print("Transfer Complete")

                running = False

    p.close()


main()
