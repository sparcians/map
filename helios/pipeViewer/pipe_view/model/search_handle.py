
import os
import time
import sys
import subprocess
from logging import debug, error, info

__SEARCH_PROGRAM_ENV_VAR_NAME = 'TRANSACTIONSEARCH_PROGRAM'
TRANSACTION_SEARCH_PROGRAM = os.environ.get(__SEARCH_PROGRAM_ENV_VAR_NAME, os.getcwd())

if os.path.isfile(TRANSACTION_SEARCH_PROGRAM):
    can_search = True
else:
    # keep looking if not explicitly stated
    # try to figure out based on transactiondb path
    __MODULE_ENV_VAR_NAME = 'TRANSACTIONDB_MODULE_DIR'
    transaction_module_path = os.environ.get(__MODULE_ENV_VAR_NAME, os.getcwd())
    build_folder_name = transaction_module_path.strip(os.path.sep).split(os.path.sep)[-1]
    TRANSACTION_SEARCH_PROGRAM = os.path.join(transaction_module_path,
                                    '../../../tools/transactionsearch',
                                    build_folder_name,
                                    'transactionsearch')
    TRANSACTION_SEARCH_PROGRAM = os.path.normpath(TRANSACTION_SEARCH_PROGRAM)
    if os.path.isfile(TRANSACTION_SEARCH_PROGRAM):
        can_search = True
    else:

        # Try with no build folder now that CMake is being used
        TRANSACTION_SEARCH_PROGRAM = os.path.join(transaction_module_path,
                                    '../../../tools/transactionsearch',
                                    'transactionsearch')
        TRANSACTION_SEARCH_PROGRAM = os.path.normpath(TRANSACTION_SEARCH_PROGRAM)
        if os.path.isfile(TRANSACTION_SEARCH_PROGRAM):
            can_search = True
        else:
            can_search = False

if can_search:
    print('transaction search initialized: you will be able to use search.')
    print('Using:', TRANSACTION_SEARCH_PROGRAM)
else:
    print('transaction search could not be found.' \
          'Try setting TRANSACTIONSEARCH_PROGRAM ' \
          'to path of your transactionsearch executable', file = sys.stderr)


# # Gets data for use by search dialog.
class SearchHandle:

    def __init__(self, context):
        # other access to db. only used for resolving location id's the strings
        self.__db = context.dbhandle.database

    # # Does search, first argument query_type takes 'string' or 'regex'
    #  @param progress_callback This is NOT wx.ProgressDialog's update function.
    #  Takes 3 arguments (percentage complete, result count, result info string).
    #  Callback returns a 2-tuple: (continue, skip)
    #  @return Results list. Each entry is a tuple of matches (start, end, loc ID, annotation)
    # TODO does this really need to launch a subprocess? can't it just make an API call to transaction db?
    def Search(self, query_type, query_string, start_tick = None, end_tick = None, locations = [], progress_callback = None, invert = False):
        assert isinstance(query_string, str)
        if not query_type in ['string', 'regex']:
            raise Exception('query type must be string or regex, got %s', query_type)
        if not can_search:
            raise Exception('There were problems with the initialization of search. You cannot search.')
        results = []
        arglist = [TRANSACTION_SEARCH_PROGRAM, \
                   self.__db.filename, \
                   query_type, \
                   query_string,
                   "1" if invert else "0"]

        # Start tick arg
        if start_tick is not None:
            arglist.append(str(start_tick))
        else:
            arglist.append(str(0))

        # End tick arg
        if end_tick is not None:
            arglist.append(str(end_tick))
        else:
            arglist.append(str(-1))

        # Location filter arg
        if len(locations) > 0:
            arglist.append(','.join([str(loc) for loc in locations]))
        else:
            arglist.append('')

        # print 'Search Arglist = ', arglist

        process = subprocess.Popen(arglist, stdout = subprocess.PIPE, stderr = subprocess.PIPE)

        process.poll()
        if process.returncode is not None and process.returncode != 0:
            raise IOError('Search subprocess failed: {}'.format(process.stderr.read()))

        cont = True
        while cont: # Loop until process terminates
            line = process.stdout.readline()
            while line:
                line_type = line[0]
                line_content = line[1:].strip()
                if line_type == b'p'[0]: # process
                    if progress_callback:
                        percent = min(99.99, float(line_content) * 100)
                        cont, skip = progress_callback(percent, len(results), '{} Potential Matches'.format(len(results)))
                        if cont is False:
                            break # early exit when cont is False
                elif line_type == b'r'[0]: # result
                    annotation_start = line_content.find(b':')
                    try:
                        start_and_loc = line_content[0:annotation_start]
                        time_range, loc_id = start_and_loc.split(b'@')
                        start, end = time_range.split(b',')
                        annotation = line_content[annotation_start + 1:]
                    except Exception as ex:
                        error('Failed to parse search output line "{}" from "{}"' \
                              .format(line_content, line_content))
                        raise
                    results.append((int(start), int(end), int(loc_id), annotation.decode('utf-8')))
                # update line
                line = process.stdout.readline()

            # Only terminate after the remainder of stdout has been processed
            # AND the return code from the previous iteration's poll was set.
            # This guarantees that all data was processed
            if process.returncode is not None:
                break

            time.sleep(0.001)
            process.poll()

        # Could wait to finish just to ensure error code is 0. Because of the
        # above code, cont=False can be an early exit when the search subprocess
        # reports 100% completion of the file. The subprocess might not be ended
        # yet though. This is just for sanity checking if enabled
        # #process.wait()

        # Check for failures after stdin has been exhausted.
        # If waiting for process completion above, there is no need to check for
        # a 'None' returncode here.
        if process.returncode != 0 and process.returncode is not None:
            print('Search subprocess returned non-zero error code: {}: {}' \
                                 .format(process.returncode, process.stderr.read()), file = sys.stderr)

        # #print 'Search results = ', len(results)

        return results

