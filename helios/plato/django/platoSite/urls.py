from django.conf import settings
from django.conf.urls import url, include
from django.conf.urls.static import static
from django.contrib import admin
from django.urls import path

from . import platoView


urlpatterns = [
    #path(r'', platoView.index, name = 'index'),
    path(r'', platoView.platoView, name = 'index'),
    #path(r'^plato', platoView.platoView, name = 'index'),
    path(r'login/', platoView.loginView, name = 'login'),
    path(r'signup/', platoView.signupView, name = 'signup'),
    #path(r"^plato/view/", platoView.platoView, name = "platoView")
    #url(r'^(?P<data>[^/]+)/$', views.data, name = 'data'),
    path(r'logout/', platoView.logoutView, name = 'logout'),
]
