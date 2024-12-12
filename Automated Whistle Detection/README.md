# Automated Whistle Detection

This code listens for whistles (detected with a bandpass filter and short term energy estimation), and if predicts one has occurced, will send a speculative stop MQTT message to the ESP32 shot clock code.

This code is based on Scott Harden's 2016 qt audio monitor project which can be found here: https://github.com/swharden/Python-GUI-examples/tree/master/2016-07-37_qt_audio_monitor

The method of whistle detection used was developed in Kathirvel et. al. 2011, which can be found here: https://www.researchgate.net/profile/Soman-Kp/publication/49607355_Automated_Referee_Whistle_Sound_Detection_for_Extraction_of_Highlights_from_Sports_Video/links/5a252175aca2727dd87e7a38/Automated-Referee-Whistle-Sound-Detection-for-Extraction-of-Highlights-from-Sports-Video.pdf


### Setup
* Install pyaudio and pyqtgraph version 5
* To run: `python3 go.py`