import paramiko
import socket
import os
import win32com.shell.shell as shell
import time
from datetime import datetime

def ethernetDisEn():
    commands=['netsh interface set interface "Ethernet" disabled','netsh interface set interface "Ethernet" enabled']
    timeouts=[1,10]
    for i in range(len(commands)):
        print(commands[i])
        shell.ShellExecuteEx(lpVerb='runas', lpFile='cmd.exe', lpParameters='/c '+commands[i])
        time.sleep(timeouts[i])

def main():
    sc=paramiko.SSHClient()
    sc.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    finished=False
    time_poll=0
    while not finished:
        if time.time()-time_poll>20:
            time.sleep(0.1)
            time_poll=time.time()
            try:
                sc.connect("10.42.0.1", port=22, username="sensar", password="Sens@r")
                fc=sc.open_sftp()
                #get log
                fc.get("/home/sensar/Desktop/receive/bin/log.txt", "C:/Users/guill/OneDrive/Documents/Sensar/Oslo/Firmwares/SICK/dev/log.txt")

                #check watchdog (time in file, read it and compare with time on computer to see if it keeps getting updated)
                fc.get("/home/sensar/Desktop/receive/watch.txt", "C:/Users/guill/OneDrive/Documents/Sensar/Oslo/Firmwares/SICK/dev/bin/watch.txt")
                with open("C:/Users/guill/OneDrive/Documents/Sensar/Oslo/Firmwares/SICK/dev/bin/watch.txt", 'r') as f:
                    l=f.readlines()[0]
                    print(f"time since last loop on RPi : {round(time.time()-float(l), 2)} s.")
                    if time.time()-float(l)>60:
                        print("warning, RPi stopped listening")
                        stdin, stdout, stderr = sc.exec_command(f"sh Desktop/rcv.sh", timeout=2)
                        stdout.channel.set_combine_stderr(True)
                        output = stdout.readlines()
                        print(output)

                #get bin files (TODO: get only missing ones instead of getting everything everytime)
                raws=fc.listdir("/home/sensar/Desktop/receive/bin/raw")
                for d in raws:
                    if "bin" not in d:
                        print(d)
                        for f in fc.listdir("/home/sensar/Desktop/receive/bin/raw/"+d):
                            if not os.path.exists(f"C:/Users/guill/OneDrive/Documents/Sensar/Oslo/Firmwares/SICK/dev/bin/raw/RPI/{d}"):
                                os.makedirs(f"C:/Users/guill/OneDrive/Documents/Sensar/Oslo/Firmwares/SICK/dev/bin/raw/RPI/{d}")
                            fc.get(f"/home/sensar/Desktop/receive/bin/raw/{d}/{f}", 
                                f"C:/Users/guill/OneDrive/Documents/Sensar/Oslo/Firmwares/SICK/dev/bin/raw/RPI/{d}/{f}")
                fc.close()
                print(f"got raw files")

                print(f"adjust time  : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
                stdin, stdout, stderr = sc.exec_command(f"sudo date -s '{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}'")
                stdout.channel.set_combine_stderr(True)
                output = stdout.readlines()
                print(output)

                print("")

                # finished=True
            except socket.gaierror:
                print("couldnt find address")
                ethernetDisEn()
            except paramiko.SSHException:
                print("server failed to execute")
            except Exception as e:
                print(f"script failed : {e}")

            sc.close()

    print("finished")

if __name__=="__main__":
    main()