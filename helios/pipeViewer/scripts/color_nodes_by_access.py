#sample script to be executed by pipeViewer Console
from model.node_element import NodeElement
highest_access_count = 0
for element in context.GetElements():
    if isinstance(element, NodeElement):
        split = element.GetProperty('data').split(':')
        if len(split) < 2:
            continue # either Start or End
        accesses = int(split[1])
        if accesses > highest_access_count:
            highest_access_count = accesses

for element in context.GetElements():
    if isinstance(element, NodeElement):
        split = element.GetProperty('data').split(':')
        if len(split) < 2:
            continue # either Start or End
        accesses = int(split[1])
        color_val = int((1 - accesses/(highest_access_count*1.0))*255)
        context.dbhandle.database.AddMetadata(element.GetProperty('name'), {'color':(color_val, 255, 255)})
