import requests
import numpy as np
import socket
import time
import plotly.express as px
from plotly.offline import plot
import pandas as pd
from datetime import datetime
import select 
import matplotlib.pyplot as plt
import os
from threading import Thread
import struct
import pyads

error=False

# function init 
def init_sock(begin):
    # sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    # sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1) #enable broadcasting mode
    #Parameters
    localPort=4210
    bufferSize=1024
    #Objects
    sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)  ## Internet,UDP
    sock.close()
    sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)  ## Internet,UDP
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR,1)
    sock.bind(('', localPort))
    if begin:
        print("UDP server : {}:{}".format(get_ip_address(),localPort))
    return sock
# function get_ip_address 
def get_ip_address():
    """get host ip address"""
    ip_address = '';
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("8.8.8.8",80))
    ip_address = s.getsockname()[0]
    s.close()
    return ip_address

def disp(msg):
        print(str(datetime.now())+'\t'+ msg.strip('\r\n'))

def plc_rot():
    try:
        plc=pyads.Connection('172.19.64.1.1.1', pyads.PORT_TC3PLC1)
        plc.open()
        k=plc.read_by_name("SNAKE.angle")
        plc.close()
        disp(str(k))
        return k
    except:
        disp("FAILED connection to pyads")
        return 0

def init_file(date, etrNum):
    d = datetime.fromtimestamp(date) 
    if not os.path.exists(f"bin/raw/{etrNum}"):
        os.makedirs(f"bin/raw/{etrNum}")
    if not os.path.exists(f"bin/raw/{etrNum}/{d.strftime('%Y-%m-%d_%H-%M-%S')}.bin"):
        disp("file did not exist, creating it....")
        with open(f"bin/raw/{etrNum}/{d.strftime('%Y-%m-%d_%H-%M-%S')}.bin", 'wb') as file:
            file.write(struct.pack('<I', date))
            file.write(struct.pack('<I', plc_rot())) #starting rotation speed
            file.write(struct.pack('<I', plc_rot())) #starting angle

# function main 
def main():
    a=True
    binGot=0
    initGot=0
    sock=init_sock(True)
    while True:
        ready=select.select([sock],[],[],1) #check if there is data waiting
        if ready[0]:
            data, addr = sock.recvfrom(1024) # get data
            msg=str(data, 'utf-8')

            disp("received message: {} from {}\n".format(data,addr))    
            disp(msg)
            with open(f"bin/log.txt", "a") as f:
                f.write(str(datetime.now())+'\t'+ msg.strip('\r\n')+"\n")

            addr=(msg.split(',')[0],4210) #get address from mthe message received
            sock.connect(addr)
            sock.sendall(b'\x01'+bytes("RPi received OK",'utf-8'))  # answer
            time.sleep(0.5)

            mode=msg.split(',')[-1] #beginning or end of measure
            etrNum=msg.split(',')[1] #system id
            date=int(msg.split(',')[2]) #date of message
            disp(f"date : {date}, mode {mode}")

            if mode=="0"and initGot!=date:
                init_file(date, etrNum)
                initGot=date
                    
            if mode=="1" and binGot!=date:
                disp("getting bin")
                init_file(date, etrNum)
                
                t, binGot=get_bin(addr[0], date, etrNum) #save binary 
                file_name=binToPng(t, date, etrNum) #plot data to html page with ploly graph
                file_name=binTohtml(t, date, etrNum) #plot data with matplotlib TODO thread to plot it since it can be long
                disp(f"new file saved : {file_name}")
            error=False
            sock.close()
            sock=init_sock(False)
            print("")


def get_bin(ip, d, etrNum):
    url_list=[f"http://{ip}/rend"]
    t=b''
    for url in url_list:
        disp(f"trying to access {url}")
        try:
            r=requests.get(url,timeout=3)
            t=r.content
            disp(f"bin got in {r.elapsed}")
        except:
            disp("FAILED to access content")
    date = datetime.fromtimestamp(d) 
    disp(f"sys number is #{etrNum}")
    if not os.path.exists("bin/raw/" + etrNum):
        os.makedirs("bin/raw/" + etrNum)

    with open(f"bin/raw/{etrNum}/{date.strftime('%Y-%m-%d_%H-%M-%S')}.bin", 'ab') as file:
        file.write(t)

    try:
        url=f"http://{ip}/stop"
        r=requests.get(url,timeout=3)
        disp(str(r.content))
    except:
        disp("FAILED to connect to stop")
    return t, d

def binTohtml(t,date, etrNum):
    tab_splitted = t.split(b'\t\r\t')[0:]
    tl=[]
    sl=[]
    for t in tab_splitted:
        # if len(t)!=1000:
            # print(len(t), t)
        # else:
        for i in range(0,1000,4):
            tl.append(int.from_bytes(t[i:i+2],byteorder='big', signed=False))
            sl.append(int.from_bytes(t[i+2:i+4],byteorder='big', signed=False))
    df=pd.DataFrame((tl,sl)).transpose()
    df.columns=["tl","sl"]
    # print(df)
    fig=px.line(df, x="tl", y="sl", title="sick measure", labels="ax", markers=True)
    # fig.show()
    date = datetime.fromtimestamp(date) 

    if not os.path.exists("bin/plot/" + etrNum):
        os.makedirs("bin/plot/" + etrNum)
    fig.write_html(f"bin/plot/{etrNum}/{date.strftime('%Y-%m-%d_%H-%M-%S')}.html")
    #plot(fig)
    return f"bin/plot/{date.strftime('%Y-%m-%d_%H-%M-%S')}.html"

def binToPng(t,date, etrNum):
    tab_splitted = t.split(b'\t\r\t')[0:]
    tl=[]
    sl=[]
    for t in tab_splitted:
        for i in range(0,1000,4):
            tl.append(int.from_bytes(t[i:i+2],byteorder='big', signed=False))
            sl.append(int.from_bytes(t[i+2:i+4],byteorder='big', signed=False))
    plt.clf()
    plt.plot(tl, sl, color='blue', linestyle="-", marker=".")
    date = datetime.fromtimestamp(date) 

    if not os.path.exists("bin/plot/" + etrNum):
        os.makedirs("bin/plot/" + etrNum)
    plt.savefig(f"bin/plot/{etrNum}/{date.strftime('%Y-%m-%d_%H-%M-%S')}.png")
    disp("png saved")
    return f"bin/plot/{date.strftime('%Y-%m-%d_%H-%M-%S')}.png"

def boot_worker():
    worker=Thread(target=main, name="worker")
    worker.start()
    return worker

def watchdog(target, action):
    disp("watchdog running")
    while True:
        if not target.is_alive():
            disp("WATCHDOG: target thread is not running, restarting...")
            target=action()

def oslopus():
    # begin=True
    # sock=init_sock(begin)
    worker=boot_worker()
    wd=Thread(target=watchdog, args=(worker, boot_worker), daemon=True, name="watchdog")
    wd.start()
    wd.join()

if __name__ == '__main__':
    os.chdir("C:/Users/sensa/OneDrive/Documents/GLe/OSLO/SICK/dev/")
    print(os.getcwd())
    oslopus()   