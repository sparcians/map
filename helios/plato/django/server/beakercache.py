'''
Created on Jul 13, 2019

@author: j.gross
'''
from beaker.cache import CacheManager
from beaker.util import parse_cache_config_options

cache_opts = {'cache.type': 'memory'}

cache = CacheManager(**parse_cache_config_options(cache_opts))
