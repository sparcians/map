'''
Created on Jun 26, 2019

@author: j.gross
'''
from channels.auth import AuthMiddlewareStack
from channels.routing import ProtocolTypeRouter, URLRouter
from django.urls import path, re_path

from .bpEndpoint import bpEndpoint
from .utilityEndpoint import utilityEndpoint


websocket_urlpatterns = [
    # url(r'^ws/polls/(?<data>[^/]+)/$', dataConsumer.DataConsumer),
    # path(r'ws/polls/<uri>/', dataConsumer.DataConsumer),
    # path(r'ws/branchPredictor/<uri>', bpEndpoint.bpEndpoint),
    re_path(r'ws/sources', bpEndpoint.as_asgi()),
    re_path(r'ws/getData', bpEndpoint.as_asgi()),
    # will handle ws/sources, ws/*
    re_path(r'ws/utility', utilityEndpoint.as_asgi())
    # path(r'ws/<uri>', bpEndpoint.bpEndpoint)
]

application = ProtocolTypeRouter({
    # (http->django views is added by default)
    'websocket': AuthMiddlewareStack(
        URLRouter(
            websocket_urlpatterns
        )
    ),
})
