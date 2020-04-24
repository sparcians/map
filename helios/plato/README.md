# Plato
Plato is a visualization framework and analysis tool suite designed for Sparta-based simulators.

The default installation and configuration instructions for Plato are minimal:
- uses sqlite as django DB
- uses django authentication

Assume Python3 and Django 3 throughout.  Plato was developed for Django 2, so there may be some lurking incompatibility bugs.

# Use conda to set up a run-time environment
You may set up your environment however you wish.  The following is an example
that uses conda miniforge:

1. Bootstrap conda miniforge

 - Download miniforge

  wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-x86_64.sh

 - Run the installer in batch mode (-b), don't modify your .bashrc (-s)

  bash ./Miniforge3-Linux-x86_64.sh -b -s -p <miniforge_install_dir>/miniforge

 - Activate the miniforge environment:

  source <miniforge_install_dir>/miniforge/bin/activate

2. Install packages required for a minimal installation

 - Install packages from default channels

  conda create -n plato wheel django-cors-headers Beaker dask numba colorama==0.4.1 hdf5plugin toolz fsspec cloudpickle uvicorn gitpython click

 - Install packages that aren't currently in default channels

  conda install -n plato -c tethysplatform -n plato django-channels channels

 - Deactivate the base environment

  conda deactivate

 - Now and in the future, use these two commands to initialize the plato environment

  source <miniforge_install_dir>/miniforge/etc/profile.d/conda.sh

  conda activate plato

# Setup and run servers

- Three different servers are required to run plato correctly
  1. a backend database, which provides storage for user data and other state.  For the minimal installation, we will just use sqlite3 DB file; no server needed
  2. the ASGI server, which includes django, creates the dynamic web pages and serves data via a websocket interface
  3. a nginx web server, which serves static assets like javascript, images, etc.

## The database server
This documentation describes the method for sqlite3 and postgres

### Sqlite3 setup
- This minimal install is configured to use sqlite3. sqlite3 is just a file: (django/db.sqlite3)
### Configuring the schema is independent of the DB
  - create the schema that the django ORM will use, from the django directory
    - cd django
    - python3 manage.py makemigrations server
    - python3 manage.py migrate

## Add username(s) for access to the plato tool
Since we aren't using external authentication, seed users into the database. (Look up the Django "admin" feature for a longer term solution.)

    - python3 manage.py shell
    - from django.contrib.auth.models import User
    - user = User.objects.create_user('ari', 'ari@universe', 'ari')
      - pick whatever user name you want.  I used 'ari'.
    - exit()

## Create a supuer user to access the Django admin interface
    - python manage.py createsuperuser

## The ASGI server
Not sure what the best practice is for starting ASGI server implementation of Django, but this seems to work.

From the plato/django directory, run this to start the django/ASGI server

  - env LOG_FILE=uvicorn.log PYTHONUNBUFFERED=1 PYTHONPATH=$PWD/../py PYTHONOPTIMIZE=1 uvicorn server.asgi:application --host 0 --port 8002 --workers 1 --loop uvloop --ws websockets --reload
  
For convenience, use the start_plato.sh script, but be sure to the edit the path to your miniforge first.

Note that this startup method runs the server in the foreground.  Ctrl-C to stop it.

## The static asset server
Let Django handle the dynamic pages and leave static assets to a basic web server to avoid bogging django down.  Here is an example nginx method.

### Install nginx
  - conda create -n nginx nginx
  - in a separate shell from where you started the ASGI server
    - conda activate nginx

### Configure nginx
The static assets are under plato/ui.  nginx was configured to serve these static assets by using the plato
directory as a PWD.  However, the conda installation makes some assumptions about the default error log.
To avoid an extraneous error on startup, make this symbol link under the plato directory:

ln -s nginx/var var

### Use the plato nginx configuration to start an nginx
  - from the plato directory
    - nginx -c nginx/nginx.conf -p $PWD
    - When you want to stop server: nginx -c nginx/nginx.conf -p $PWD -s stop
    - start_nginx.sh is provided for convenience 
      - edit path to miniforge
      - to start nginx:
        -  start_nginx.sh
      - to stop nginx:
        -  start_nginx.sh stop
      - read the script for more options
  - notes:
    - the default nginx conf included with plato runs a virtual host on port 8000 that simply redirects users to the plato login and runs the static asset server on port 8080.  I don't think the port 8000 redirect is necessary
    - Since this example uses conda and implies plato is the only user of this nginx, it might make more sense to just use the conda environment's nginx configuration.  On the other hand, this method is designed to take advantage of an OS-installed nginx where the plato user may not have permission to adjust the OS ngnix

# Time to access plato from a browser
 - navigate to 'http://localhost:8002/plato?dataSourceDir=<absolute path to plato/demo/data>' and login
 
# Plato documentation
For general plato documentation, see `docs/`

# postgres support
plato was originally implemented with postgres as the database
A postgres installation and the psycopg2-binary package are required

# Authentication options
The minimal installation example above uses the native django authentication, which
stores its credentials in its configured database.

## Active Directory
Plato was developed to use Active Directory (LDAP) for authentication.  That implementation is in django/server/authenticationAD.py
It looks like there might have been an initial attempt to use django_auth_ldap, since that 3rd party package is referenced in settings.py.
In fact, plato may have been originally developed to authenticate
against LDAP. When the customer switched to Active Directory, then django_auth_ldap might have been in place, but authenticationAD.py was written too override the default Django authentication backend called ModelBackend.

Note: authenticationAD.py disables the password checking by using an `if true` statement. Read the comments in the code.

To re-enable Active Directory, these might be the steps (or more
than the necessary steps and also less than the necessary steps):


1. install the following packages into OS or conda environment

2. remove the `if True` statements and `fix things` exception handling in django/server/authenticationAD.py

3. django/settings.py

 - uncomment the `import ldap` and `from django_auth_ldap` lines 

 - uncomment `django_auth_ldap` from `INSTALLED_APPS`

 - uncomment `AUTHENTICATION_BACKENDS` `LDAP_AUTH_SETTINGS` and `AUTH_LDAP`*.  Note that these might not all have been used in the Active Directory implementation.

4. in platoSite/platoView.py:
 - uncomment "from server.authenticationAD import ActiveDirectoryBackend"
 - switch the authenticate to ActiveDirectoryBackend().authenticate per the comments in the file
 
5. Notes
 - Might have to invoke Django migrate

 - Hindsight: For active Directory support, perhaps all that is needed is to `import ldap` and update platoView.py and fix the code in django/server/authenticationAD.py

### Local user creation under Active Directory
To create test users not in the Active Directory, follow these steps rather than the similar looking ones above:
  - python3 manage.py shell  
  - from server.models import AuthenticatedUser
  - AuthenticatedUser.objects.create(username='<newuser>')
  - exit()

## Okta
"okta-django-samples" repo on GitHub seems to be a good starting point, although it is only tested on later versions of Django 2

# Known issues
- One has to specify the datasource directory in the URL.  There is a TODO in viewer.js for adding a dialog to allow the user to select a datasource directory
- django admin screen hangs after entering credentials
- Plato will periodically try to save the user layouts which result in a "django.core.exceptions.SynchronousOnlyOperation: You cannot call this from an async context - use a thread or sync_to_async."
