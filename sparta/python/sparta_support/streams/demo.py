import streaming

# Example Python subclass which prints simulation
# statistics data to the console
class stdout(streaming.Stream):
    def __init__(self):
        pass

    def initialize(self, stream_root):
        pass

    def processStreamPacket(self, packet):
        print 'demo.stdout Python sink just got some data:'
        print ", ".join(str(x) for x in packet)

    # The stream manager singleton relies on a static
    # 'factoryCreate()' method to instantiate one of
    # these objects, after only being given the Python
    # class:
    #
    #   stream_config.top.core0.rob.streamTo(demo.stdout)
    @staticmethod
    def factoryCreate():
        return stdout()

