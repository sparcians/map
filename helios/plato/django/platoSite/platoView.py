import logging

from django import forms
from django.contrib.auth import login, authenticate, logout
from django.contrib.auth.decorators import login_required
from django.contrib.auth.models import User
from django.contrib.auth.forms import UserCreationForm
from django.http.response import HttpResponseRedirect
from django.shortcuts import render
from django.template.context import RequestContext
from django.views.decorators.csrf import ensure_csrf_cookie

import git
from server import settings
#from server.authenticationAD import ActiveDirectoryBackend


logger = logging.getLogger("plato.view")


@login_required(login_url = '/plato/login/')
def platoView(request):
    '''
    serve the main plato page
    '''
    try:
        sha1 = git.Repo(search_parent_directories = True).head.object.hexsha
    except Exception as e:
        # TODO not sure what this might throw, catch all for now
        logger.exception(e)
        sha1 = "00000000"

    return render(request, 'plato/viewer.html', {'userName': str(request.user.username),
                                                 'currentHost': request.get_host().split(':')[0],
                                                 'assetPort': settings.NGINX_PORT,
                                                 'asgiServer': request.get_host(),
                                                 'sha1Value': sha1[0:8],
                                                 'sha1Long': sha1,
                                                 })


def loginView(request):
    '''
    serve the login page
    '''
    logout(request)
    username = password = ""
    if request.POST:
        username = request.POST['username']
        password = request.POST['password']

#       Switch from using ActiveDirectory Backend to the simple default Django user list in the database
#       TODO: Make authentication source configurable in Django so we can support AD, LDAP, Okta
#       Theoretically django.contrib.auth.authenticate() is supposed to try all the configured Backends
#       until it successfully authenticates the user. The use of ActiveDirectoryBackend().authenticate()
#       might have been a bit of a hack to get the demo working.
#        user = ActiveDirectoryBackend().authenticate(username, password)
        user = authenticate(username=username, password=password)
        if user is not None and user.is_active:
            user.backend = 'django.contrib.auth.backends.ModelBackend'
            login(request, user)
            return HttpResponseRedirect(request.POST.get("next", request.GET.get("next", '/plato/')))
    return render(request, 'plato/login.html', {'currentHost': request.get_host().split(':')[0],
                                                'next': request.GET.get("next", request.POST.get("next", '/plato/')),
                                                'assetPort': settings.NGINX_PORT,
                                                })
    # return render_to_response('plato/login.html', {}, RequestContext(request))

def signupView(request):
    if request.method == 'POST':
        form = UserCreationForm(request.POST)
        if form.is_valid():
            form.save()
            username = form.cleaned_data.get('username')
            raw_password = form.cleaned_data.get('password1')
            user = authenticate(username=username, password=raw_password)
            login(request, user)
            return HttpResponseRedirect(request.POST.get("next", request.GET.get("next", '/plato/')))
    else:
        form = UserCreationForm()
    return render(request, 'plato/signup.html', {'currentHost': request.get_host().split(':')[0],
                                                'next': request.GET.get("next", request.POST.get("next", '/plato/')),
                                                'assetPort': settings.NGINX_PORT,
                                                'form': form
                                                })

def logoutView(request):
    '''
    logout and send back to login page
    '''
    logout(request)
    return render(request, 'plato/login.html', {'currentHost': request.get_host().split(':')[0],                                                'next': request.GET.get("next", request.POST.get("next", '/plato/')),
                                                'assetPort': settings.NGINX_PORT,
                                                })

