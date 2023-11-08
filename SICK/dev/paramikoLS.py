import paramiko
import win32com.shell.shell as shell

sc=paramiko.SSHClient()
sc.set_missing_host_key_policy(paramiko.AutoAddPolicy())
sc.connect("10.42.0.1", port=22, username="sensar", password="Sens@r")
fc=sc.open_sftp()
raws=fc.listdir("/home/sensar/Desktop/receive/bin/raw")
for d in raws:
    if "bin" not in d:
        print(d)


