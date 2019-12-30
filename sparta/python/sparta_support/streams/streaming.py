# Base class for all streaming Python sinks
class Stream:
    def __init__(self):
        pass
    def initialize(self, stream_root):
        raise NotImplementedError
    def processStreamPacket(self, packet):
        raise NotImplementedError

# Collection of stream objects organized as:
#   { 'stream_node_path', python_sink_obj },
#   { '      ...       ',       ...       },
#   ...
class StreamManager():
    def __init__(self):
        self._streams = {}
        self._stream_paths = {}

    # C++ calls this method, giving us a source node and
    # a Python class wrapper we can use to instantiate a
    # Python sink object, and store the source/sink pair
    # in a dictionary.
    def addStream(self, cpp_source_node, py_sink_class):
        sink = py_sink_class.factoryCreate()

        # Store the sink in the dictionary by its full path.
        # Note that we can have more than one sink being fed
        # from the same C++ source node, so the dictionary
        # is a mapping from node path to a list of sinks.
        path = cpp_source_node.getFullPath()
        if path in self._streams:
            self._streams[path].append(sink)
        else:
            self._streams[path] = [sink]

        # Store a mapping from source path to the wrapped
        # source object we can request data from later on.
        self._stream_paths[path] = cpp_source_node

        # One-time initialization call to the Python subclass
        sink.initialize(cpp_source_node)

        # Message printout
        print 'A new Python sink has been added:'
        print '\t' + str(sink)
        print '\t' + str(cpp_source_node)

    # Get all buffered data from the C++ source nodes, and
    # send those data packets down to the Python sinks.
    def processStreams(self):
        for source_path,sinks in self._streams.iteritems():
            if source_path not in self._stream_paths:
                print 'Could not get the source object for path: ' + source_path
                continue
            source_node = self._stream_paths[source_path]

            # Call to C++ to get the data since our last update
            data = source_node.getBufferedData()
            if len(data) > 0:
                # There is some more data. Send it to the clients.
                for sink in sinks:
                    sink.processStreamPacket(data)

