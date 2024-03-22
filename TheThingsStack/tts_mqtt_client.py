# pip install paho-mqtt

import paho.mqtt.subscribe as subscribe

m = subscribe.simple(topics=['#'], hostname="eu1.cloud.thethings.network", port=1883, auth={'username':"icicle-monitor",'password':"NNSXS.V7RI4..."}, msg_count=2)
for a in m:
    print(a.topic)
    print(a.payload)
