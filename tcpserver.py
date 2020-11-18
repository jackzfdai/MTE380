# This example code is in the Public Domain (or CC0 licensed, at your option.)

# Unless required by applicable law or agreed to in writing, this
# software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.

# -*- coding: utf-8 -*-

import socket
import sys
import threading

# -----------  Config  ----------
IP_VERSION = 'IPv4'
PORT = 3333
# -------------------------------
occupancy_count = 100
print("Occupancy Count is: " + str(occupancy_count))

def clicker_thread(conn):
    global occupancy_count
    while True:
        data = conn.recv(128)
        if not data:
            break
        occupancy_count = occupancy_count - 1 
        print("Occupancy Count is: " + str(occupancy_count))
        # data = data.decode()
        # print('Received data: ' + data)
        # reply = 'OK: ' + data
        # conn.send(reply.encode())
    conn.close()

if IP_VERSION == 'IPv4':
    family_addr = socket.AF_INET
elif IP_VERSION == 'IPv6':
    family_addr = socket.AF_INET6
else:
    print('IP_VERSION must be IPv4 or IPv6')
    sys.exit(1)


try:
    sock = socket.socket(family_addr, socket.SOCK_STREAM)
except socket.error as msg:
    print('Error: ' + str(msg[0]) + ': ' + msg[1])
    sys.exit(1)

print('Socket created')

clicker_thread_list = []

sock.bind(('', PORT))
print('Socket binded')

while True:
    try:
        sock.listen(1)
        print('Socket listening')
        conn, addr = sock.accept()
        print('Connected by', addr)
        clicker_handler_thread = threading.Thread(target=clicker_thread, args=(conn,))
        clicker_thread_list.append(clicker_handler_thread)
        clicker_handler_thread.start() 
    except socket.error as msg:
        print('Error: ' + str(msg[0]) + ': ' + msg[1])
        sock.close()
        sys.exit(1)


# while True:
#     data = conn.recv(128)
#     if not data:
#         break
#     occupancy_count = occupancy_count - 1 
#     print("Occupancy Count is: " + str(occupancy_count))
#     # data = data.decode()
#     # print('Received data: ' + data)
#     # reply = 'OK: ' + data
#     # conn.send(reply.encode())
# conn.close()
