import requests
#import numpy as np
import socket
import time
#import plotly.express as px
#from plotly.offline import plot
#import pandas as pd
from datetime import datetime
import select 
#import matplotlib.pyplot as plt
import os

#Parameters
localPort=4210
bufferSize=1024
#Objects
sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)  ## Internet,UDP
error =False

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
    s.connect(("chg.me.local",80))
    ip_address = s.getsockname()[0]
    s.close()
    return ip_address

def disp(msg):
        print(str(datetime.now())+'\t'+ msg.strip('\r\n'))

# function main 
def main(sock):
    a=True
    binGot=0
    while True:
        #sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)  ## Internet,UDP
        with open("watch.txt", 'w') as f:
            f.write(str(time.time()))
        ready=select.select([sock],[],[],1)
        if ready[0]:
            
            data, addr = sock.recvfrom(1024) # get data
            # disp("received message: {} from {}\n".format(data,addr))    
            msg=str(data, 'utf-8')
            disp(msg)
            with open(f"bin/log.txt", "a") as f:
                f.write(str(datetime.now())+'\t'+ msg.strip('\r\n')+"\n")

            sock.connect(addr)
            sock.sendall(b'\x01'+bytes("RPi received OK",'utf-8'))  # write data
            time.sleep(0.1)

            mode=msg.split(',')[-1]
            etrNum=msg.split(',')[0]
            date=int(msg.split(',')[1])
            disp(f"date : {date}, mode {mode}")

            if mode=="1" and binGot!=date:
                disp("getting bin")
                t, binGot=get_bin(addr, date, etrNum)
                #file_name=binToPng(t, date)
                # file_name=binTohtml(t, date)
                #disp(f"new file saved : {file_name}")
            error=False 
            sock.close()
            sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)  ## Internet,UDP
            sock.bind(('', localPort))
            print("")

def get_bin(addr, d, etrNum):
    url_list=[f"http://{addr[0]}/rend"]
    for url in url_list:
        disp(f"trying to access {url}")
        r=requests.get(url)
        t=r.content
    date = datetime.fromtimestamp(d) 
    disp(f"sys number is #{etrNum}")
    if not os.path.exists("bin/raw/" + etrNum):
        os.makedirs("bin/raw/" + etrNum)
    with open(f"bin/raw/" + etrNum + f"/{date.strftime('%Y-%m-%d_%H-%M-%S')}.bin", 'wb') as file:
        file.write(t)
    disp(f"bin got in {r.elapsed}")
    return t, d

if __name__ == '__main__':
    print(os.getcwd())
    os.chdir("/home/sensar/Desktop/receive")
    print(os.getcwd())
    while True:
        try:    #added
            init()
            main(sock)
        except Exception as e:
            if not error:
                with open(f"bin/log.txt", "a") as f:
                    f.write(f"script failed : {e}"+"\n")
                error=True
            print(f"script failed : {e}"+"\n")
            time.sleep(2)
