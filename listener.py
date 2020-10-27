#!./env/bin/python3

import struct
import wave

import matplotlib.animation as animation
import matplotlib.pyplot as plt
import numpy as np
import pyaudio

import contextlib
import os
import subprocess
import sys
import socket
import time

SAVE = 0.0
TITLE = ''
WIDTH = 1280
HEIGHT = 720
FPS = 25.0

MAX_y = 16384
RESOLUTION_Y = 32
nFFT = 64 * 3
BUF_SIZE = 4 * nFFT
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 44100

HOST = '127.0.0.1'  
PORT = 1488         

def main():

  dpi = plt.rcParams['figure.dpi']
  plt.rcParams['savefig.dpi'] = dpi
  plt.rcParams["figure.figsize"] = (1.0 * WIDTH / dpi, 1.0 * HEIGHT / dpi)

  # Frequency range
  x_f = 1.0 * np.arange(-nFFT / 2 + 1, nFFT / 2) / nFFT * RATE

  p = pyaudio.PyAudio()
  # Used for normalizing signal. If use paFloat32, then it's already -1..1.
  # Because of saving wave, paInt16 will be easier.
  MAX_y = 2.0 ** (p.get_sample_size(FORMAT) * 8 - 1)

  frames = None
  wf = None
  if SAVE:
    frames = int(FPS * SAVE)
    wf = wave.open('temp.wav', 'wb')
    wf.setnchannels(CHANNELS)
    wf.setsampwidth(p.get_sample_size(FORMAT))
    wf.setframerate(RATE)

  stream = p.open(format=FORMAT,
                  channels=CHANNELS,
                  rate=RATE,
                  input=True,
                  frames_per_buffer=BUF_SIZE,
                  input_device_index=0)
  
  with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 384)
    conn, addr = s.accept()
    with conn:
      while True:

        N = int(max(stream.get_read_available() / nFFT, 1) * nFFT)
        data = stream.read(N, exception_on_overflow=False)

        y = np.array(struct.unpack("%dh" % (N * CHANNELS), data)) / MAX_y
        y_L = y[::2]
        y_R = y[1::2]
  
        Y_L = np.fft.fft(y_L, nFFT)
        Y_R = np.fft.fft(y_R, nFFT)

        Y = abs(np.hstack((Y_L[(int(-nFFT / 2) -1):-1], Y_R[:int(nFFT / 2)])))
  
        for ind in range(len(Y)):
          Y[ind] = Y[ind] * RESOLUTION_Y
          if Y[ind] > RESOLUTION_Y:
            Y[ind] = RESOLUTION_Y  
  
        out = ""
        
        multipliers = [1, 1, 1, 1, 1, 2, 3, 4, 5, 6, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8]
        
        for i in range(32):
          c = Y[i*3+32*3]
          l = c
          r = c
          if i*3+32*3-1>32*3:
            l = Y[i*3+32*3-1]
          if i*3+32*3+1<64*3:
            r = Y[i*3+32*3+1]
          Y[i*3+32*3] = (c) * multipliers[i]
          if Y[i*3+32*3] > 32:
            Y[i*3+32*3] = 32
          out += (str(int(Y[i*3+32*3])) + " ")
          if int(Y[i*3+32*3]) < 10:
            out += " "
        
        if not conn.recv(1):
          break
        
        conn.sendall(bytes(out, 'utf-8'))
        
  stream.stop_stream()
  stream.close()
  p.terminate()

@contextlib.contextmanager
def silence():
  devnull = os.open(os.devnull, os.O_WRONLY)
  old_stderr = os.dup(2)
  sys.stderr.flush()
  os.dup2(devnull, 2)
  os.close(devnull)
  try:
    yield
  finally:
    os.dup2(old_stderr, 2)
    os.close(old_stderr)

def engage_command(
  command = None
  ):
  process = subprocess.Popen(
    [command],
    shell      = True,
    executable = "/bin/bash")
  process.wait()
  output, errors = process.communicate()
  return output

if __name__ == "__main__":
  with silence():
    main()

