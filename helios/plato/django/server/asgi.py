'''
ASGI entrypoint. Configures Django and then runs the application defined in the
ASGI_APPLICATION setting.
'''

import asyncio
import os

from channels.routing import get_default_application
import django
from server.settings import ASYNCIO_DEBUG


if ASYNCIO_DEBUG:
    loop = asyncio.get_event_loop()
    loop.set_debug(True)
    loop.slow_callback_duration = 0.2

os.environ.setdefault('DJANGO_SETTINGS_MODULE', "server.settings")

django.setup()

application = get_default_application()
