import sys
import time
import os

# Setup import path for transactiondb module
TRANSACTION_MODULE_DIR = os.environ.get('TRANSACTION_MODULE_DIR', None)
if TRANSACTION_MODULE_DIR:
    sys.path.append(TRANSACTION_MODULE_DIR)

try:
    import transactiondb
except Exception as e:
    print >> sys.stderr, 'Failed to import transactiondb: {0}'.format(e)
    print >> sys.stderr, 'TRANSACTION_MODULE_DIR was: {0}'.format(TRANSACTION_MODULE_DIR)
    raise

print sys.modules['transactiondb']

TRANSACTION_FILE = os.environ['TRANSACTION_FILE']

from check_interval_end import check_interval_endings

# Try opening a non-existant file and ensure the exception is caught
NONEXISTANT_FILE = '/dev/null/nonexistant_file'
try:
    w = transactiondb.IntervalWindow(NONEXISTANT_FILE)
except Exception as e:
    print('Successfully caught bad construction with exception: {0} type: {1}' \
          .format(e, type(e)))
    pass
else:
    raise RuntimeError('Should have failed to open "{0}"'.format(NONEXISTANT_FILE))

window = transactiondb.IntervalWindow(TRANSACTION_FILE)
print(window)
print(str(window))
print(repr(window))

assert window.getFileStart() < 1000
assert window.getFileEnd() > 1000
assert window.getWindowLeft() < 1000
assert window.getWindowRight() > 1000

results = window.query(250)
print(results)
print(str(results))
print(repr(results))
print('Number of results: {0}'.format(len(results)))

time.sleep(1.0)

real = None
prox = None
max_loc = 0
max_id = 0
for idx,trans in enumerate(results):
    max_loc = max(trans.getLocationID(), max_loc)
    max_id = max(trans.getTransactionID(), max_id)

    if idx == 10:
        print trans.getType()
        print trans.getTypeString()
        real = trans.makeRealCopy()
        prox = trans.makeProxy()
    
print('max_locid {0}'.format(max_loc))
print('max_transid {0}'.format(max_id))

# Both safe until IntervalList is destroyed. After, they are dangerous to access
# and they don't know it (segfaults could occur)
print real
print prox

assert not real.isProxy()
assert real.isValid()
assert prox.isProxy()
assert prox.isValid()

check_interval_endings(window)

print('Destroying IntervalList')
results._destroy()

print('Destroying and IntervalWindow')
window._destroy()

# Must be able to print destroyed objects just to show their destroyed state
print window
print results

# Must not be able to access once destroyed.
# Should really try every method to be sure
try:
    results.getLength()
except Exception as e:
    print('Caught access to destroyed IntervalList as expected')
else:
    raise RuntimeError('Should have failed to access destroyed IntervalList')

try:
    window.getFileStart()
except Exception as e:
    print('Caught access to destroyed IntervalWindow as expected')
else:
    raise RuntimeError('Should have failed to access destroyed IntervalWindow')

del results
del window

print 'Success'
print 'Used transactiondb lib from: {0}'.format(sys.modules['transactiondb'])
exit(0)
