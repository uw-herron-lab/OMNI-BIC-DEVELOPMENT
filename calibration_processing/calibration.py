import numpy as np
import scipy.signal as signal
import matplotlib.pyplot as plt
from decimal import Decimal
import csv

def load_data(file_path):

    with open(file_path, "r") as file:
        reader = file.readlines()
        clean_data = []
        str_len = len(reader[1][0:-1])
        clean_data.append(reader[0][0:-1])
        for i in np.arange(1,len(reader)-1):
            clean_data.append(reader[i][0:-1])
        split_data = []
        for line in clean_data:
            split_data.append(line.split(","))
        non_str_data = []
        non_str_data.append(split_data[0])
        for row in np.arange(1,len(split_data)):
            non_str_data.append([split_data[row][0], Decimal(split_data[row][1]), int(split_data[row][2])])

    return non_str_data

def separate_ch_sig(data, num_channels=16):
    samples = int((len(data))/num_channels)
    emgData = []
    for ch in np.arange(16):
        ch_signals = []
        for samp_ind in np.arange(ch,len(data),num_channels):
            ch_signals.append(data[samp_ind])
        if ch == 0:
            emgData = ch_signals
            emgData = np.reshape(emgData, (1, np.shape(emgData)[0], np.shape(emgData)[1]))
        else:
            emgData = np.concatenate((emgData, [ch_signals]), axis = 0)
    
    return emgData


# Define a function to apply a bandpass filter (20-500 Hz is a typical range for EMG)
def bandpass_filter(data, lowcut=20, highcut=450, fs=2000, order=2):
    scale_factor = 1e6
    data = np.array(data, dtype=np.float64) * scale_factor
    nyquist = fs/2
    low = lowcut/nyquist
    high = highcut/nyquist
    sos = signal.butter(order, [low,high], btype='band', output='sos')
    b, a = signal.butter(order, [low,high], btype='band', output='ba')
    z, p, k = signal.butter(order, [low,high], btype='band', output='zpk')
    filt_data = signal.sosfiltfilt(sos, data)
    print("b: ", b)
    print("a: ", a)
    print("z:", k)
    return filt_data / scale_factor

# Rectify the EMG signal (absolute value)
def rectify(data):
    return np.abs(data)


def lowpass_filter(data, lowcut=40, fs=2000, order=1):
    scale_factor = 1e6
    nyquist = fs/2
    low = lowcut/nyquist
    data = np.array(data, dtype=np.float64) * scale_factor
    b, a = signal.butter(order, low, btype='low', output='ba')
    z, p, k = signal.butter(order, low, btype='low', output='zpk')
    filt_data = signal.filtfilt(b, a, data)
    print("b: ", b)
    print("a: ", a)
    print("z:", k)
    return filt_data / scale_factor