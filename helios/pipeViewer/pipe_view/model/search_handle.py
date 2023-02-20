from __future__ import annotations
import os
import time
import sys
import subprocess
from logging import error
from typing import Callable, List, Optional, Tuple, TYPE_CHECKING
import shutil

if TYPE_CHECKING:
    from .layout_context import Layout_Context

__SEARCH_PROGRAM_ENV_VAR_NAME = 'TRANSACTIONSEARCH_PROGRAM'
TRANSACTION_SEARCH_PROGRAM = os.environ.get(__SEARCH_PROGRAM_ENV_VAR_NAME,
                                            shutil.which("transactionsearch"))
print("INFO: looking for ", TRANSACTION_SEARCH_PROGRAM)

can_search = False
if os.path.isfile(TRANSACTION_SEARCH_PROGRAM):
    can_search = True
else:
    # keep looking if not explicitly stated
    # try to figure out based on transactiondb path
    __MODULE_ENV_VAR_NAME = 'TRANSACTIONDB_MODULE_DIR'
    env_var = os.environ.get(__MODULE_ENV_VAR_NAME)
    if env_var is None:
        added_path = os.path.join(
            os.path.dirname(__file__),
            "../../../../release/helios/pipeViewer/transactionsearch"
        )
        added_path = os.path.abspath(added_path)
        can_search = True
        if not os.path.isdir(added_path):
            can_search = False
        transaction_module_path = added_path
    else:
        transaction_module_path = os.environ.get(__MODULE_ENV_VAR_NAME,
                                                 os.getcwd())

    build_folder_name = \
        transaction_module_path.strip(os.path.sep).split(os.path.sep)[-1]
    TRANSACTION_SEARCH_PROGRAM = os.path.join(transaction_module_path,
                                              'transactionsearch')
    TRANSACTION_SEARCH_PROGRAM = os.path.normpath(TRANSACTION_SEARCH_PROGRAM)
    if os.path.isfile(TRANSACTION_SEARCH_PROGRAM):
        can_search = True

if can_search:
    print('transaction search initialized: you will be able to use search.')
    print('Using:', TRANSACTION_SEARCH_PROGRAM)
else:
    print('transaction search could not be found.'
          'Try setting TRANSACTIONSEARCH_PROGRAM '
          'to path of your transactionsearch executable', file=sys.stderr)


# Gets data for use by search dialog.
class SearchHandle:

    def __init__(self, context: Layout_Context) -> None:
        # other access to db. only used for resolving location id's the strings
        self.__db = context.dbhandle.database

    # Does search, first argument query_type takes 'string' or 'regex'
    #  @param progress_callback This is NOT wx.ProgressDialog's update function
    #  Takes 3 arguments (% complete, result count, result info string)
    #  Callback returns a 2-tuple: (continue, skip)
    #  @return Results list. Each entry is a tuple of matches
    #  (start, end, loc ID, annotation)
    # TODO does this really need to launch a subprocess? can't it just make an
    # API call to transaction db?
    def Search(self,
               query_type: str,
               query_string: str,
               start_tick: Optional[int] = None,
               end_tick: Optional[int] = None,
               locations: List[int] = [],
               progress_callback: Optional[Callable] = None,
               invert: bool = False) -> List[Tuple[int, int, int, str]]:
        assert isinstance(query_string, str)
        if query_type not in ('string', 'regex'):
            raise Exception(
                f'query type must be string or regex, got {query_type}'
            )
        if not can_search:
            raise Exception(
                'There were problems with the initialization of search. '
                'You cannot search.'
            )
        results: List[Tuple[int, int, int, str]] = []
        arglist = [TRANSACTION_SEARCH_PROGRAM,
                   self.__db.filename,
                   query_type,
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
        if locations:
            arglist.append(','.join([str(loc) for loc in locations]))
        else:
            arglist.append('')

        # print 'Search Arglist = ', arglist

        process = subprocess.Popen(arglist,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)

        process.poll()
        if process.returncode is not None and process.returncode != 0:
            assert process.stderr is not None
            stderr = process.stderr.read().decode("utf-8")
            raise IOError(f'Search subprocess failed: {stderr}')

        cont = True
        assert process.stdout is not None
        while cont:  # Loop until process terminates
            line = process.stdout.readline()
            while line:
                line_type = line[0]
                line_content = line[1:].strip()
                if line_type == b'p'[0]:  # process
                    if progress_callback:
                        percent = min(99.99, float(line_content) * 100)
                        num_results = len(results)
                        cont, skip = progress_callback(
                            percent,
                            num_results,
                            f'{num_results} Potential Matches'
                        )
                        if cont is False:
                            break  # early exit when cont is False
                elif line_type == b'r'[0]:  # result
                    annotation_start = line_content.find(b':')
                    try:
                        start_and_loc = line_content[0:annotation_start]
                        time_range, loc_id = start_and_loc.split(b'@')
                        start, end = time_range.split(b',')
                        annotation = line_content[annotation_start + 1:]
                    except Exception:
                        line_str = line_content.decode('utf-8')
                        error('Failed to parse search output line "%s"',
                              line_str)
                        raise
                    results.append((int(start),
                                    int(end),
                                    int(loc_id),
                                    annotation.decode('utf-8')))
                # update line
                line = process.stdout.readline()

            # Only terminate after the remainder of stdout has been processed
            # AND the return code from the previous iteration's poll was set.
            # This guarantees that all data was processed
            if process.returncode is not None:
                break

            time.sleep(0.001)
            process.poll()

        # Check for failures after stdin has been exhausted.
        # If waiting for process completion above, there is no need to check
        # for a 'None' returncode here.
        if process.returncode != 0 and process.returncode is not None:
            assert process.stderr is not None
            stderr = process.stderr.read().decode("utf-8")
            print('Search subprocess returned non-zero error code: '
                  f'{process.returncode}: {stderr}',
                  file=sys.stderr)

        return results
