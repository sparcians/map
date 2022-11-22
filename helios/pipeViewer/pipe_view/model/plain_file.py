import shlex
## reads a file created by neato -Tplain <file> > outfile
class Plain:
    # indices into data
    NODE_POSX = 0
    NODE_POSY = 1
    NODE_DATA = 4

    def __init__(self, filename: str) -> None:
        # objects keyed by identifiers
        self.nodes = {}
        self.edges = []
        self.bounds = (0.0, 0.0)
        s = open(filename)
        for line in s:
            fields = shlex.split(line)
            # field 0 is type of line
            if fields[0] == 'node':
                self.nodes[fields[1]] = fields[2:]
            elif fields[0] == 'edge':
                self.edges.append((fields[1], fields[2]))
            elif fields[0] == 'graph':
                self.bounds = (float(fields[2]), float(fields[3]))
        s.close()

