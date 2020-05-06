# Test for NRUCache
# WARNING: This test is time-sensitive so debugging may be complicated. The cache should really use some kind of
#          "time provider" that allows simulated advancement of time rather than the wall-clock timer.

import sys
from os import path
import time

sys.path.append(path.split(path.dirname(__file__))[0])

from plato.backend.common import NRUCache

NRUCache.EXPIRATION_DEFAULT_MS = 450  # short expiration for this test

c = NRUCache(4)  # Capacity 4

assert(len(c) == 0)

assert (1 not in c)
assert (2 not in c)
assert (3 not in c)
assert (4 not in c)

c[1] = 1
c[2] = 2
c[3] = 3
c[4] = 4
c[5] = 5  # Will not remove [1] because its not expired
assert (1 in c)

c.expire(1)  # Allow 1 to be replaced by 5 without waiting for the appropriate amount of time
c[5] = 5  # Will finally remove [1] because last write expired it

# 1 should have been evicted because of capacity
assert (1 not in c)
assert (2 in c)
assert (3 in c)
assert (4 in c)
assert (5 in c)

# Allow anything to be replaced now
c.expire_all()

c[2] = 2
c[6] = 6

assert (1 not in c)
assert (3 not in c)
assert (2 in c)
assert (4 in c)
assert (5 in c)
assert (6 in c)

assert (not c.is_expiring(2))
assert (not c.is_expiring(6))

c[4] = 4
c[5] = 6

# Nothing should be expiring since it was all touched recently
assert (not c.is_expiring(2))
assert (not c.is_expiring(4))
assert (not c.is_expiring(5))
assert (not c.is_expiring(6))

print('\nsleep...\n')
time.sleep(1)

# Touching [2] will mark all others as expiring since 1s has passed.
# When each other is touched here, they will be marked as non-expiring once again.
_ = c[2]
_ = c[5]
_ = c[6]

# Since [4] was not touched, it is still in expiring state
print('check that 4 is expiring')
assert (c.is_expiring(4))

print('\nsleep...\n')
time.sleep(0.2)

assert (2 in c)
assert (4 in c)
assert (5 in c)
assert (6 in c)

# Touch everyone again. Note that 4 will not be removed because enough time has not elapsed after marking as expired
_ = c[2]
assert (4 in c)
_ = c[4]
_ = c[5]
_ = c[6]

print('\nsleep...\n')
time.sleep(1)

assert (4 in c)

# Let 5 be marked as expiring since we don't touch it
_ = c[2]
_ = c[4]
_ = c[6]

assert (2 in c)
assert (4 in c)
assert (5 in c)
assert (6 in c)
assert (c.is_expiring(5))

print('\nsleep...\n')
time.sleep(1)

# Since 5 is not touched first here, it will be removed because we slept for more than the expiration interval after 5
# was marked as expiring
_ = c[2]
_ = c[4]
_ = c[6]

assert (2 in c)
assert (4 in c)
assert (5 not in c)
assert (6 in c)

assert(len(c) == 3)
