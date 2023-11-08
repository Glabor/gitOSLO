#!/usr/bin/env python
# -*- coding: utf-8 -*-
#Libraries
import socket    #https://wiki.python.org/moin/UdpCommunication
import select 

#Parameters
localPort=4210
bufferSize=1024
#Objects
sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)  ## Internet,UDP
# function init 
def init():
    # sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    # sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1) #enable broadcasting mode
    sock.bind(('', localPort))
    print("UDP server : {}:{}".format(get_ip_address(),localPort))
# function get_ip_address 
def get_ip_address():
    """get host ip address"""
    ip_address = '';
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("8.8.8.8",80))
    ip_address = s.getsockname()[0]
    s.close()
    return ip_address

# function main 
def main():
    a=True
    while True:
        ready=select.select([sock],[],[],1)
        if ready[0]:
            
            data, addr = sock.recvfrom(1024) # get data
            print("received message: {} from {}".format(data,addr))  
            msg =  str(data, 'utf-8')
            print(float(msg)*3.3/4096*2+.3)
            if a:
                sock.sendto(b'\x01'+bytes("RPi received OK",'utf-8'),addr)  # write data
                a=not a
            else:
                sock.sendto(b'\x00'+bytes("RPi received OK",'utf-8'),addr)  # write data
                a=not a

if __name__ == '__main__':
    init()
    main()