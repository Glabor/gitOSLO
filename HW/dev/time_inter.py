import pandas as pd
import numpy as np

df=pd.read_csv('test/hw.csv')
df.columns=("time", "x", "y", "z")
print(len(df["time"]))

inter=[]
for i in range(1,len(df["time"])):
    inter.append(df["time"][i]-df["time"][i-1])
    if df["time"][i]-df["time"][i-1]>20:
        print(df["time"][i]-df["time"][i-1],df["time"][i])
print(np.mean(inter), np.max(inter), np.mean(inter))