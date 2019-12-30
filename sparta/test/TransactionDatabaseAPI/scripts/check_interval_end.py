import os
import sys

# Setup import path for transactiondb module
TRANSACTION_DB_DIR = os.environ.get('TRANSACTION_MODULE_DIR', None)
if TRANSACTION_DB_DIR:
    sys.path.append(TRANSACTION_DB_DIR)

try:
    import transactiondb
except Exception as e:
    print >> sys.stderr, 'Failed to import transactiondb: {0}'.format(e)
    print >> sys.stderr, 'TRANSACTION_DB_DIR was: {0}'.format(TRANSACTION_DB_DIR)
    raise

##! Checks the interval endpoint to ensure intervals are not being returned in
#   queries when their right ends match. Right endpoints should be exclusive
def check_interval_endings(window):
    # Sample some of the db to ensure that unexpected interavals aren't showing up
    errors = 0
    for tick in xrange(0,300):
        rs = window.query(tick)
        for tr in rs:
            if tr.getRight() == tick:
                errors += 1
                print >> sys.stderr, 'Found unexpected transaction {0} with end={1} when stabbing at ' \
                                     '{2}. This should not be allowed' \
                                     .format(tr, tr.getRight(), tick)
            else:
                pass
                print 'Found OK transaction: {0} stabbing at {1}'.format(tr, tick)

    if errors > 0:
        raise Exception('Found {0} transactions with invalid endpoints. IntervalWindow API and ' \
                        'ISLs should be filtering or preventing these' \
                        .format(errors))

if __name__ == '__main__':
    TRANSACTION_FILE = os.environ['TRANSACTION_FILE']
    window = transactiondb.IntervalWindow(TRANSACTION_FILE)
    check_interval_endings(window)
