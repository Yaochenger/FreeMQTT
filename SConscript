import os
import rtconfig
from building import *

cwd = GetCurrentDir()

src = Split('''
api/mqtt_api.c
core/core_mqtt.c
core/core_mqtt_state.c
core/core_mqtt_serializer.c
demo/demo.c
port/port.c
''')

path =  [cwd]
path += [cwd + '/api']
path += [cwd + '/port']
path += [cwd + '/core/include']
path += [cwd + '/core/interface']

print(path)

group = DefineGroup('FreeMQTT', src, depend = [''], CPPPATH = path)

Return('group')
