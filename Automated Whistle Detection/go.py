from PyQt5 import QtGui, QtCore, QtWidgets

import sys
import numpy as np
import pyqtgraph
import SWHear
import random
import scipy
import paho.mqtt.client as paho
from paho import mqtt

whistle_count = 0
prev_whistle = False
end_of_range = False
size_of_range = 0
ste = 0
range_gap = 0
published = False

STE_THRESHOLD = 300000000

# setting callbacks for different events to see if it works, print the message etc.
def on_connect(client, userdata, flags, rc, properties=None):
    """
        Prints the result of the connection with a reasoncode to stdout ( used as callback for connect )

        :param client: the client itself
        :param userdata: userdata is set when initiating the client, here it is userdata=None
        :param flags: these are response flags sent by the broker
        :param rc: stands for reasonCode, which is a code for the connection result
        :param properties: can be used in MQTTv5, but is optional
    """
    print("CONNACK received with code %s." % rc)


# with this callback you can see if your publish was successful
def on_publish(client, userdata, mid, properties=None):
    """
        Prints mid to stdout to reassure a successful publish ( used as callback for publish )

        :param client: the client itself
        :param userdata: userdata is set when initiating the client, here it is userdata=None
        :param mid: variable returned from the corresponding publish() call, to allow outgoing messages to be tracked
        :param properties: can be used in MQTTv5, but is optional
    """
    print("SPEC MSG " + str(mid))


# print which topic was subscribed to
def on_subscribe(client, userdata, mid, granted_qos, properties=None):
    """
        Prints a reassurance for successfully subscribing

        :param client: the client itself
        :param userdata: userdata is set when initiating the client, here it is userdata=None
        :param mid: variable returned from the corresponding publish() call, to allow outgoing messages to be tracked
        :param granted_qos: this is the qos that you declare when subscribing, use the same one for publishing
        :param properties: can be used in MQTTv5, but is optional
    """
    print("Subscribed: " + str(mid) + " " + str(granted_qos))


class Whistle_Detector:
    def __init__(self):
       
        ### Setting up MQTT Client ###
        self.client = paho.Client(client_id="", userdata=None, protocol=paho.MQTTv5)
        self.client.on_connect = on_connect
        self.client.tls_set(tls_version=mqtt.client.ssl.PROTOCOL_TLS)
        self.client.username_pw_set("xxx", "xxx")
        self.client.connect("xxx", 8883)
        self.client.loop_start()

        self.client.on_subscribe = on_subscribe
        self.client.on_publish = on_publish
        
        ### Setting up Filter ###
        sampling_frequency = 44100
        passband = [3750, 4100]
        stopband = [3650, 4200]
        gpass = 1
        gstop = 40
        N, Wn = scipy.signal.buttord(
            [passband[0] / (0.5 * sampling_frequency),
            passband[1] / (0.5 * sampling_frequency)],
            [stopband[0] / (0.5 * sampling_frequency),
            stopband[1] / (0.5 * sampling_frequency)],
            gpass,
            gstop,
        )
        self.sos = scipy.signal.butter(N, Wn, analog=False, btype='band', output='sos')
        self.ear = SWHear.SWHear(rate=44100, updatesPerSecond=20)
        self.ear.stream_start()

    def update(self):
        global end_of_range
        global size_of_range
        global ste
        global range_gap
        global published

        if not self.ear.data is None:
            filt_data = scipy.signal.sosfilt(self.sos, self.ear.data)
            ste_current = np.sum(np.square(filt_data))
            ste += ste_current
            size_of_range += 1
            if ste_current < STE_THRESHOLD:
                range_gap += 1
            else:
                range_gap = 0

            if range_gap > .25 * size_of_range:
                end_of_range = True

            if (not published) and size_of_range > 50 and ste/size_of_range > STE_THRESHOLD:
                published = True
                self.client.publish("/game/", payload="4", qos=1)

            if end_of_range:
                if ste/size_of_range > STE_THRESHOLD:
                    if(size_of_range > 50): #50
                        print("WHISTLE!!:"," ste: ", ste/size_of_range," range size ", size_of_range)
                    else:
                        print("ste: ", ste/size_of_range)
                        print("range size ", size_of_range)
                ste = 0
                size_of_range = 0
                range_gap = 0
                end_of_range = False
                published = False #only 1 msg per range
            

        QtCore.QTimer.singleShot(1, self.update) # QUICKLY repeat

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    form = Whistle_Detector()
    form.update()  # start with something
    app.exec_()
    print("DONE")
