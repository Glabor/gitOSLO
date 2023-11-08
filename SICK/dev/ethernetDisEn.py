import os
import win32com.shell.shell as shell
import time

commands=['netsh interface set interface "Ethernet" disabled','netsh interface set interface "Ethernet" enabled']
for c in commands:
    print(c)
    shell.ShellExecuteEx(lpVerb='runas', lpFile='cmd.exe', lpParameters='/c '+c)
    time.sleep(3)