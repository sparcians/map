import sys
import time
import os

# Setup import path for transactiondb module
TRANSACTION_MODULE_DIR = os.environ.get('TRANSACTION_MODULE_DIR', None)
if TRANSACTION_MODULE_DIR:
    sys.path.append(TRANSACTION_MODULE_DIR)

try:
    import transactiondb2 as tdb2
except Exception as e:
    print >> sys.stderr, 'Failed to import transactiondb2: {0}'.format(e)
    print >> sys.stderr, 'TRANSACTION_MODULE_DIR was: {0}'.format(TRANSACTION_MODULE_DIR)
    print >> sys.stderr, 'sys.path was {0}'.format(sys.path)
    raise

print sys.modules['transactiondb2']

TRANSACTION_FILE = os.environ['TRANSACTION_FILE']

from check_interval_end import check_interval_endings

# Try opening a non-existant file and ensure the exception is caught
NONEXISTANT_FILE = '/dev/null/nonexistant_file'
try:
    w = tdb2.TransactionDatabase(NONEXISTANT_FILE, 2000)
except Exception as e:
    print('Successfully caught bad construction with exception: {0} type: {1}' \
          .format(e, type(e)))
    pass
else:
    raise RuntimeError('Should have failed to open "{0}"'.format(NONEXISTANT_FILE))

window = tdb2.TransactionDatabase(TRANSACTION_FILE, 2000)
print(window)
print(str(window))
print(repr(window))

# Make a query to kick off the background loading
window.query(0,1, lambda t,q: true)

time.sleep(2.0) # Allow data to load

assert window.getFileStart() < 1000, window.getFileStart()
assert window.getFileEnd() > 1000, window.getFileEnd()
assert window.getWindowLeft() < 1000, window.getWindowLeft()
assert window.getWindowRight() > 1000, window.getWindowRight()

results = []
def query_callback(tick, qobj):
    ##print 'Query at ', tick
    results.append([tick] +
                   [qobj.getTransactionID(i) for i in range(1800)[::10]]

                   #[qobj.getTransactionAnnotation(3),
                   # qobj.getTransactionAnnotation(12),
                   # qobj.getTransactionAnnotation(33),
                   # qobj.getTransactionAnnotation(45),
                   # qobj.getTransactionAnnotation(51),
                   )

print window.getLocationMap()

window.query(0, 250, query_callback)

import pprint
for r in results:
    print r

print('Number of results: {0}'.format(len(results)))

print('Locations: {0}'.format(window.getLocationMap()))

print(window)
print('Cached Annotations: {0}'.format(window.getNumCachedAnnotations()))

time.sleep(1.0)

##real = None
##prox = None
##max_loc = 0
##max_id = 0
##for idx,trans in enumerate(results):
##    max_loc = max(trans.getLocationID(), max_loc)
##    max_id = max(trans.getTransactionID(), max_id)
##
##    if idx == 10:
##        print trans.getType()
##        print trans.getTypeString()
##        real = trans.makeRealCopy()
##        prox = trans.makeProxy()
##
##print('max_locid {0}'.format(max_loc))
##print('max_transid {0}'.format(max_id))
##
### Both safe until IntervalList is destroyed. After, they are dangerous to access
### and they don't know it (segfaults could occur)
##print real
##print prox
##
##assert not real.isProxy()
##assert real.isValid()
##assert prox.isProxy()
##assert prox.isValid()
##
##check_interval_endings(window)
##
##print('Destroying IntervalList')
##results._destroy()

print('Destroying and IntervalWindow')
window._destroy()

# Must be able to print destroyed objects just to show their destroyed state
print window

### Must not be able to access once destroyed.
### Should really try every method to be sure
##try:
##    results.getLength()
##except Exception as e:
##    print('Caught access to destroyed IntervalList as expected')
##else:
##    raise RuntimeError('Should have failed to access destroyed IntervalList')

try:
    window.getFileStart()
except Exception as e:
    print('Caught access to destroyed IntervalWindow as expected')
else:
    raise RuntimeError('Should have failed to access destroyed IntervalWindow')

del window

print 'Success'
print 'Used transactiondb2 lib from: {0}'.format(sys.modules['transactiondb2'])
exit(0)
