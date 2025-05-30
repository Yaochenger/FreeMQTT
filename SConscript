import os
import rtconfig
from building import *

Import('SDK_LIB')

cwd = GetCurrentDir()

src = Split('''
core_mqtt.c
core_mqtt_state.c
core_mqtt_serializer.c
port/port.c
demo.c
''')

path =  [cwd]
path += [cwd + '/include']
path += [cwd + '/interface']
path += [cwd + '/port']

print(path)

group = DefineGroup('firemqtt', src, depend = [''], CPPPATH = path)

Return('group')
