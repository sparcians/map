# Reads a specific old version of branch traces needed for branch predictor analysis

import numpy as np
import pandas as pd
import h5py
import struct
from struct import Struct

# Rudimentary rader/converter for makeshift fiat shp branch/training binary traces.
# Reads and converts to a hdf5 file incrementally to maintain low memory when converting even multi-GB output files.
# Produces two datasets in the output hdf file: 
class SHPTraceToHDF5:

    RECORD_TYPE_FMT = Struct('<c')
    BRANCH_TRAIN_RECORD_FMT = Struct('<QQ??Qihih?B??Q')
    TABLE_WEIGHT_TRAIN_RECORD_FMT = Struct('<iiihhhbb')

    # Branch training event (numpy/pandas storage)
    BRANCHES_DTYPE = np.dtype([
        ('trn_idx',                      'u4'), # Index of this event. Dask indexing is not what you'd expect
        ('cycle',                        'u8'),
        ('pc',                           'u8'),
        ('correct',                      'b'),
        ('taken',                        'b'),
        ('tgt',                          'u8'),
        ('yout',                         'i4'),
        ('bias_at_lookup',               'i2'),
        ('theta_at_training',            'i4'),
        ('bias_at_training',             'i2'),
        ('shpq_weights_found',           'b'),
        ('dynamic_state',                'i1'),
        ('indirect',                     'b'),
        ('uncond',                       'b'),
        ('instructions',                 'u8'),
        ('my_first_weight_update_index', 'i4'), # Index of first weight update for this branch. -1 for none. # TODO: make this a flag to use latest_weight_update_index
        ('latest_weight_update_index',   'i4'), # Index of latest weight update as of this branch. 
                                              # If this branch has no weight update events, this points to the
                                              # next weight update index in the table following this branch
        ])

    # Weight update event (numpy/pandas storage)
    WEIGHTS_DTYPE = np.dtype([
        ('wup_idx',       'u4'), # Index of this event. Dask indexing is not what you'd expect
        ('table',         'i4'),
        ('row',           'i4'),
        ('bank',          'i4'),
        ('lookup_weight', 'i2'),
        ('new_weight',    'i2'),
        ('d_weight',      'i2'),
        ('d_unique',      'i1'),
        ('thrash_1',      'i1'),
        ('branch_index',  'u4'), # Index for branch training event
        ])


    MAX_RECORD_SIZE = max(BRANCH_TRAIN_RECORD_FMT.size,
                          TABLE_WEIGHT_TRAIN_RECORD_FMT.size)

    # Minimum buffer size required to try a read (unless at EOF)
    MIN_BUFFER_SIZE = MAX_RECORD_SIZE + RECORD_TYPE_FMT.size

    # Size of HDF chunks on disk
    HDF_CHUNK_SIZE = 100000

    #print('Branch training record size: {} bytes'.format(BRANCHES_DTYPE.itemsize))
    #print('Weight update event record size: {} bytes'.format(WEIGHTS_DTYPE.itemsize))
    #print('Max record size: {}'.format(self.MAX_RECORD_SIZE))

    # HDF5 dataset keys
    BRANCH_TRAIN_EVENT_DS = 'training_events'
    WEIGHT_UPDATE_EVENT_DS = 'weight_updates'

    def __init__(self, compression='gzip'):
        self.compression = compression

    def convert(self, input_filename, target_file_hdf5):
        with open(input_filename, 'br') as f:
            #with open(target_file_numpy, 'wb') as npf:
                with h5py.File(target_file_hdf5, 'w') as hf:

                    # Setup the h5py HDF5 file
                    # lzf is very fast and compresses a bit
                    # gzip compresses better but takes more time
                    hf.create_dataset(self.BRANCH_TRAIN_EVENT_DS, (0,), maxshape=(None,),
                        dtype=self.BRANCHES_DTYPE, chunks=(self.HDF_CHUNK_SIZE,), compression=self.compression)
                    hf.create_dataset(self.WEIGHT_UPDATE_EVENT_DS, (0,), maxshape=(None,),
                        dtype=self.WEIGHTS_DTYPE, chunks=(self.HDF_CHUNK_SIZE,), compression=self.compression)
            
                    # total counters
                    total_brncount = 0 # branch training events
                    total_wupcount = 0 # weight update events
                    total_bytes_read = 0

                    # Read & check the header
                    header_fmt = Struct('<bIII')
                    buf = f.read(header_fmt.size)
                    self.version, self.ntables, self.nrows, self.nbanks = header_fmt.unpack_from(buf, 0)
                    total_bytes_read += header_fmt.size

                    if self.version != 4:
                        print('opened file with version ' + self.version + ' instead of expected 4')


                    # Read buffer and offset
                    buf = bytes()
                    buf_off = 0

                    # Create fixed-size arrays as buffers for the data. This is the size of incremental hdf5 writes
                    WRITE_SIZE = 5000000
                    branches = np.empty(WRITE_SIZE, dtype=self.BRANCHES_DTYPE) # branch training events
                    weights = np.empty(WRITE_SIZE, dtype=self.WEIGHTS_DTYPE) # weight update events

                    # current chunk counters
                    brncount = 0
                    wupcount = 0

                    while True:
                        # Fill buffer it below low-watermark
                        while len(buf) - buf_off < self.MIN_BUFFER_SIZE:
                            #print('only have {}/{} bytes needed for reading'.format(len(buf)-buf_off, self.MAX_RECORD_SIZE))
                            buf = buf[buf_off:]
                            buf_off = 0

                            new_bytes = f.read(32*1024*1024)
                            if len(new_bytes) == 0:
                                break # Continue processing last bytes in buffer. If not enough, error will be caught
                            else:
                                buf += new_bytes
                                total_bytes_read += len(new_bytes)
                                print('Read {} more bytes from file. Buffer is now {}'.format(len(new_bytes), len(buf)))

                        # Read record type
                        rec_type, = self.RECORD_TYPE_FMT.unpack_from(buf, buf_off)
                        rec_type = rec_type[0] # From single-item byte array to int
                        buf_off += 1

                        # Process record
                        if rec_type == ord('T'): # Branch training
                            try:
                                cycle, pc, correct, taken, tgt, yout, bias_at_lookup, theta_at_training, \
                                    bias_at_training, shpq_weights_found, dynamic_state, indirect, uncond, instructions = \
                                    self.BRANCH_TRAIN_RECORD_FMT.unpack_from(buf, buf_off)

                                buf_off += self.BRANCH_TRAIN_RECORD_FMT.size
                                
                                branches[brncount] = (total_brncount, cycle, pc, correct, taken, tgt, yout, bias_at_lookup, theta_at_training, \
                                    bias_at_training, shpq_weights_found, dynamic_state, indirect, uncond, instructions, -1, total_wupcount)
                                brncount += 1
                                total_brncount += 1

                                # Note that -1 is written as a placeholder for first corresponding weight-update-event index.
                                # This requires us to keep at least 1 item in branches so that the next weight update event can write to it.
                                # TODO: The file writer should deal with this pointer, not the converter.

                            except struct.error as ex:
                                print('Stopping reading because a record could not be read. May be a truncated file at buf={} off={}: {}' \
                                    .format(len(buf), buf_off, ex))
                                sys.stdout.flush()
                                break

                        elif rec_type == ord('W'): # Weight table update
                            try:
                                table, row, bank, lookup_weight, new_weight, d_weight, d_unique, thrash_1 = \
                                    self.TABLE_WEIGHT_TRAIN_RECORD_FMT.unpack_from(buf, buf_off)

                                buf_off += self.TABLE_WEIGHT_TRAIN_RECORD_FMT.size

                                # Update latest branch with a reference to this weight update entry indx
                                if branches[brncount-1]['my_first_weight_update_index'] == -1:
                                    branches[brncount-1]['my_first_weight_update_index'] = total_wupcount
                                    ##print('TEMP: wupcount {} to branch {}'.format(total_wupcount, total_brncount-1))

                                # Refer to latest branch entry index
                                branch_index = total_brncount - 1

                                weights[wupcount] = (total_wupcount, table, row, bank, lookup_weight, new_weight, d_weight, d_unique, thrash_1, branch_index)
                                wupcount += 1
                                total_wupcount += 1

                            except struct.error as ex:
                                print('Stopping reading because a record could not be read. May be a truncated file at buf={} off={}: {}' \
                                    .format(len(buf), buf_off, ex))
                                sys.stdout.flush()
                                break

                        else:
                            raise Exception('Unknown record type: {}'.format(rec_type))


                        # Flush to disk occasionally
                        if (brncount > 1 and brncount % WRITE_SIZE == 0) or (wupcount > 1 and wupcount % WRITE_SIZE == 0):
                            print('Read {} branches, {} weight-updates from {} bytes so far. Expected pandas/numpy size: {} + {} bytes' \
                                    .format(total_brncount, total_wupcount, total_bytes_read,
                                        self.BRANCHES_DTYPE.itemsize * total_brncount,
                                        self.WEIGHTS_DTYPE.itemsize * total_brncount))
                            sys.stdout.flush()

                            # Write branch training events, but save 1 so it can be updated with weight-update event indices
                            if brncount > 1:
                                hf[self.BRANCH_TRAIN_EVENT_DS].resize((hf[self.BRANCH_TRAIN_EVENT_DS].shape[0] + brncount - 1), axis=0)
                                hf["training_events"][-(brncount-1):] = branches[:brncount-1]

                                branches[0] = branches[brncount-1] # Move latest branch event to index 0
                                brncount = 1

                            # Write weight updates
                            if wupcount > 0:
                                hf[self.WEIGHT_UPDATE_EVENT_DS].resize((hf[self.WEIGHT_UPDATE_EVENT_DS].shape[0] + wupcount), axis=0)
                                hf["weight_updates"][-wupcount:] = weights[:wupcount]

                                wupcount = 0

                            print('Chunk written')

                    # Done with loop. write any remaining data out
                    if brncount > 0:
                        hf[self.BRANCH_TRAIN_EVENT_DS].resize((hf[self.BRANCH_TRAIN_EVENT_DS].shape[0] + brncount), axis=0)
                        hf["training_events"][-brncount:] = branches[:brncount]
                        brncount = 0                

                    if wupcount > 0:
                        hf[self.WEIGHT_UPDATE_EVENT_DS].resize((hf[self.WEIGHT_UPDATE_EVENT_DS].shape[0] + wupcount), axis=0)
                        hf["weight_updates"][-wupcount:] = weights[:wupcount]
                        wupcount = 0

    

if __name__ == '__main__':
    import sys
    input_filename = sys.argv[1]
    target_filename_hdf5 = sys.argv[2]
    SHPTraceToHDF5(compression=None).convert(input_filename, target_filename_hdf5)
