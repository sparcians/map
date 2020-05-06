import asyncio
from collections import OrderedDict
import logging
import math
from threading import Lock, RLock
import threading
import time

import numba

try:
    from polls.models import LongFunctionCall
except ImportError as e:
    LongFunctionCall = None

logger = logging.getLogger("plato.backend.common")


VARIABLE_STAT_SEPARATOR = '.'  # table stats will look like "table[x].statname"

# Stat type classifications. Must match ui's common.js
STAT_TYPE_SUMMABLE = 'delta'  # A delta value that is logically summable across many items
STAT_TYPE_COUNTER = 'counter'  # A counter value where a given value is always the latest 'value' at that time
STAT_TYPE_ATTRIBUTE = 'attribute'  # An attribute of a row that is not a counter and does not make sense to sum


# Convert a color [r,g,b] where each element is an integer to a css hex color string (e.g. '#rrggbb')
def color_to_hex(c):
    r = format(c[0], 'x')
    if len(r) < 2:
        r = '0' + r
    g = format(c[1], 'x')
    if len(g) < 2:
        g = '0' + g
    b = format(c[2], 'x')
    if len(b) < 2:
        b = '0' + b

    return '#' + r + g + b


# Interpolate between two 'colors' smoothly
@numba.jit(nopython=True, nogil=True)
def interp_color(a, b, fraction):
    nc = (
        math.floor(math.sqrt((a[0]**2 + (b[0]**2 - a[0]**2) * fraction))),
        math.floor(math.sqrt((a[1]**2 + (b[1]**2 - a[1]**2) * fraction))),
        math.floor(math.sqrt((a[2]**2 + (b[2]**2 - a[2]**2) * fraction)))
    )
    return nc

# Look up an index in a dtype by name
def get_dtype_idx(dtype, target_name):
    for i,name in enumerate(dtype.names):
        if target_name == name:
            return i
    return -1


# Not-recently used cache. Evicts things that haven't been used in a while (wall-clock-time) regardless of need for
# space. Becomes a LRU-cache if capacity is hit, though if capacity is reached the cache should be enlarged since this
# is intended to store information requested in batches. If one piece of the batch cannot fit in the cache that is a
# performance problem and there will be thrashing.
#
# The expiration time here is very coarse. This is intended for tiny caches
#
# Removal works as such:
#   While over capacity, oldest items are generally removed with the caveat that expiring elements might be artificially
#   kept alive due the details of the implementation. This is OK since capacity is a safety feature only. If this
#   happens often, the cache should just be be made larger.
#
#   When setting or accessing an item, a clean is triggered
#     In this clean, any items `expiration_s` older than the next item are marked as expired and re-inserted into the
#     data structure as if newly modified.
#     If such an expired item is once again found to be older than `expiration_s` compared to the next item, it will be
#     removed.
#
#  The removal policy allows a batch of requests to touch the cache after a long period of time without immediately
#  deleting everything upon the first access.
#
# The nature of cleaning old requests this way requires that the cache not be very large.
class NRUCache:
    EXPIRATION_DEFAULT_MS = 60000

    def __init__(self, capacity=10, expiration_ms=None):
        if expiration_ms is None:
            expiration_ms = NRUCache.EXPIRATION_DEFAULT_MS

        self._capacity = capacity
        self._items = OrderedDict()  # key -> [access_time, expired, value]
        self._expiration_s = expiration_ms / 1000
        self._last_clean = 0
        self._cache_lock = Lock()

    def __len__(self):
        return len(self._items)

    def __contains__(self, item):
        return item in self._items

    def __getitem__(self, k):
        with self._cache_lock:
            v = self._items[k][2]  # grab value

            del self._items[k]
            self._items[k] = [time.time(), 0, v]  # Update access time and reinsert into ordered dict

            self._clean()

            return v

    def __setitem__(self, k, v):
        with self._cache_lock:
            if k in self._items:
                del self._items[k]  # remove to reinsert as newest item
            else:
                if len(self._items) >= self._capacity:
                    # Find the oldest thing already set as expired if possible and remove it to make room
                    for ik, (it, ie, iv) in self._items.items():
                        if ie > 0:
                            del self._items[ik]
                            break  # Freed up some room
                    else:
                        # Unable to replace anything because nothing was expired

                        # Maintain capacity by skipping inertion of this item. This is usually better policy than removing
                        # the oldest because this kind of cache, when full, implies that ALL the items are active. Since the
                        # items tend to be requested round-robin by plato widgets, this means that elements which will be
                        # needed soon will be evicted, guaranteeing waste.
                        #
                        # Since the work for k,v was already done, its less costly to drop it and keep upcoming data in the
                        # cache. Code below will accelerate expiration to help push things out of the cache faster.
                        logger.debug(f'NRUC refusing to add an item due to over-capacity k={k} items={len(self._items)} capacity={self._capacity}. Should consider making this cache larger since it is full of actively used items')

                        # Accelerate expiration of everything so items can be removed sooner if no longer used.
                        # Things that are still in use should get unexpired during the next use.
                        # _clean() will clean expire/remove as appropriate.
                        for ik, (it, ie, iv) in self._items.items():
                            expire_reduction = self._expiration_s / 5.0  # Reduce the expiration by 1/5 of expiration.
                            self._items[ik] = [it - expire_reduction, 0, iv]

                        # Still need to clean out expired items
                        # Force clean to try and expire the time-adjusted items even if last clean was very recent
                        self._clean(force=True)

                        return

            self._items[k] = [time.time(), 0, v]

            self._clean()

    # Expire a key if not expired and reset its update time. This is a test tool. Reinserts item at end of order.
    def expire(self, k):
        t = time.time()
        for ik, (it, ie, iv) in self._items.items():
            if ik == k:
                self._items[ik] = [t, 1, iv]  # expired

    # Expire a key if not expired and reset its update time. This is a test tool. Reinserts item at end of order.
    def expire_all(self):
        t = time.time()
        for ik, (it, ie, iv) in self._items.items():
            self._items[ik] = [t, 1, iv]  # expired

    # Is an element in the cache marked as expiring currently? Argument must be in the cache or an exception is thrown
    def is_expiring(self, k):
        return self._items[k][1]

    # Prune old (based on time) items from the cache
    def _clean(self, force=False):
        now = time.time()

        # Skip cleaning unless more time has elapsed
        if not force and now - self._last_clean < self._expiration_s:
            return

        self._last_clean = now

        if len(self._items) == 0:
            return

        # Expire everything that is `expiration_s` older than the current time. Remove things expired with enough time
        # elapsed again.
        to_remove = []
        to_expire = []
        for k, (t, e, v) in self._items.items():
            age = now - t
            if age > self._expiration_s:
                # If the last element is old, mark it as expired, restart the timer, and keep it around until the
                if self._items[k][1] == 1:
                    logger.debug('NRUC removing k={} {}'.format(k, now))
                    to_remove.append(k)
                elif e == 0:  # Not yet expiring
                    logger.debug('NRUC expiring k={} {}'.format(k, now))
                    to_expire.append(k)
            #else:
            #    break  # early out since iterating from oldest to newest

        for k in to_remove:
            del self._items[k]

        for k in to_expire:
            t, e, v = self._items[k]

            # Update access time, mark expired, and reinsert into ordered dict as if it is new. Note that this
            # makes the eviction policy not LRU if capacity is exceeded because this is technically an older item than
            # what might get evicted. That is ok, the point of this class is to get rid of things not recently used
            del self._items[k]
            self._items[k] = [now, 1, v]

        #print('Remaining after clean')
        #for k, (t, e, v) in self._items.items():
        #    print(k, t, e)
        #print('')

def synchronized(lock):
    '''
    synchronized decorator
    '''
    def wrap(func):
        def syncFunction(*args, **kwargs):
            with lock:
                return func(*args, **kwargs)

        return syncFunction

    return wrap

def synchronizedFine(lockDict, lock):
    '''
    value based synchronization
    '''
    def wrap(func):
        def syncFunction(*args, **kwargs):
            print(f"wait for lock {args[0]}")
            with lock:
                if args[0] in lockDict:
                    print(f"got a lock {args[0]}")
                    waitLock = lockDict[args[0]]
                else:
                    waitLock = RLock()
                    print(f"made a lock {args[0]} {waitLock}")
                    waitLock.acquire()
                    lockDict[args[0]] = waitLock
            print(f"wait for waitLock {args[0]}")
            with waitLock:
                try:
                    print(f"run func {args[0]}")
                    return func(*args, **kwargs)
                finally:
                    print(f"done {args[0]}")

        return syncFunction

    return wrap


def logtimeAsync(func):
    '''
    time logging decorator for asynchronous methods
    '''

    async def process(func, *args, **kwargs):
        if asyncio.iscoroutinefunction(func):
            return await func(*args, **kwargs)
        else:
            return func(*args, **kwargs)

    async def helper(*args, **kwargs):
        start = time.time()
        result = await process(func, *args, **kwargs)
        elapsed = time.time() - start
        if elapsed * 1000 > 800:
            logger.info(f"{func.__name__}() took {(elapsed) * 1000:.3f}ms")

            if LongFunctionCall is not None:
                # returns the object and a bool as to whether it was created or not
                newCallObj, _ = LongFunctionCall.objects.get_or_create(functionName = func.__name__,
                                                                             args = str(args),
                                                                             kwargs = str(kwargs),
                                                                             execTime = elapsed)
        return result

    return helper


countactive_lock = Lock()
countactive_items = {}
def countactive(method):
    '''
    Track the active instances of this function and log when the count changes
    '''
    name = method.__name__

    def func(*args, **kwargs):
        with countactive_lock:
            n = countactive_items.get(name, 0)
            countactive_items[name] = n + 1
            logger.debug(f"request {name} started. now has {n + 1} active")

        try:
            method(*args, *kwargs)
        finally:
            with countactive_lock:
                n = countactive_items[name]
                countactive_items[name] = n - 1
                logger.debug(f"request {name} ended. now has {n - 1} active")

    return func

def logtime(logger):
    '''
    decorator to log the time taken of a function if it exceeds 800ms
    '''

    def logtimeBase(method):
        '''
        time logging decorator for synchronous methods
        '''

        def timed(*args, **kwargs):
            '''
            the actual function that does timing around the function call
            '''
            start = time.time()
            originalName = threading.currentThread().name
            threading.currentThread().name = method.__name__

            result = method(*args, **kwargs)
            threading.currentThread().name = originalName
            elapsed = time.time() - start
            if elapsed * 1000 > 800:
                logger.warn(f"{method.__module__}::{method.__name__}() took {(elapsed) * 1000:.0f}ms")
                # logger.debug(f"{method.__name__}() args=({args}), kwargs=({kwargs}) took {(elapsed) * 1000:.2f}ms")

                if LongFunctionCall is not None:
                    newCallObj, created = LongFunctionCall.objects.get_or_create(functionName = method.__name__,
                                                                                 args = "", # str(args),
                                                                                 kwargs = "", # str(kwargs),
                                                                                 execTime = elapsed)
            return result

        return timed

    return logtimeBase
