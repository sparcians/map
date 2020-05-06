from os import path
import time
import pprint

import sys
sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.datasources.branch_training_trace.datasource import BranchTrainingDatasource


if len(sys.argv) == 1:
	filename = path.join(path.dirname(__file__), 'test-branch-training-trace.hdf5')
else:
	filename = sys.argv[1]

print ('Using file {}'.format(filename))

# Just load extents. Do this before opening the entire file to make sure there are no benefits to having the file open
# and loaded that might influence timing of this.
t = time.time()
info = BranchTrainingDatasource.get_source_information(filename)
dt = time.time() - t
print('Getting source information took {:.3f} s for {}'.format(dt, filename))
pprint.pprint(info)


# Load source data. Note that for big 1GB+ data-sets this can take 20+ seconds.
t = time.time()
branch_hm_data = BranchTrainingDatasource(filename)
dt = time.time() - t
print('Opening file took {:.3f} s for {}'.format(dt, filename))


