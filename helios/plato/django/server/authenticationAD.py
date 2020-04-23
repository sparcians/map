# Authenticate to ActiveDirectory server
#   It is unclear if this ever worked as intended.
#   Parts of the code were commented out to facilitate demos w/o a working AD server
#
#   A typical convention for AD is to bind with special user credentials that have
#   search privileges on the LDAP server. The actual authentication of the user
#   attempting a login comes after the bind.  However this code and some other examples
#   on the internet just consider a valid bind with user credentials as sufficient
#   authentication. Consider using https://django-auth-ldap.readthedocs.io/en/latest/index.html
#
import logging

from django.conf import settings
from django.contrib.auth.backends import ModelBackend
from django.contrib.auth.models import User
import ldap

from .models import AuthenticatedUser


logger = logging.getLogger("plato.auth")


class ActiveDirectoryBackend(ModelBackend):

    def authenticate(self, username = None, password = None):
        if not self.isValid(username, password):
            logger.info('From AD user invalid')
            return None
        logger.info(f'From AD user valid ({username})')
        try:
            user = User.objects.get(username = username)
            logger.info('From AD user object found')
        except User.DoesNotExist:
            try:
                logger.info('From AD user does not exist')
                ldapClient = ldap.initialize(settings.AUTH_LDAP_SERVER_URI)
                ldapClient.simple_bind_s('%s\\%s' % ('YOUR_DOMAIN_NAME', username), password)
                logger.info('From AD binding')
                # result = ldapClient.search_ext_s(settings.AD_SEARCH_DN,ldap.SCOPE_SUBTREE,
                #        "sAMAccountName=%s" % username,settings.AUTH_LDAP_USER_SEARCH)[0][1]
                result = ldapClient.search_ext_s("ou=Users,ou=Accounts,dc=<domain component0>,dc=<domain component1>,dc=<domain component2>,dc=com",
                                                                  ldap.SCOPE_SUBTREE, "sAMAccountName=%s" % username)[0][1]
                logger.info('From AD searching')
                ldapClient.unbind_s()
            except Exception as e:
                logger.exception(e)

                if True:
                    # WARNING, this is a workaround if there is no AD or LDAP auth server available
                    result = {'givenName': ['Fix'],
                              'sn': ['Me'],
                              'mail': ['Soon']}

            # givenName == First Name
            if 'givenName' in result:
                first_name = result['givenName'][0]
            else:
                first_name = None

            # sn == Last Name (Surname)
            if 'sn' in result:
                last_name = result['sn'][0]
            else:
                last_name = None

            # mail == Email Address
            if 'mail' in result:
                email = result['mail'][0]
            else:
                email = None

            user = User(username = username, first_name = first_name, last_name = last_name, email = email)
            user.is_staff = False
            user.is_superuser = False
            user.set_password(password)
            user.save()
        return user

    def getUser(self, user_id):
        try:
            return User.objects.get(pk = user_id)
        except User.DoesNotExist:
            return None

    def isValid(self, username = None, password = None):
        # # Disallowing null or blank string as password
        # # as per comment: http://www.djangosnippets.org/snippets/501/#c868
        if password == None or password == '':
            return False
        try:
            # test to see if the user is in the local db
            _ = AuthenticatedUser.objects.get(username = username)
            logger.warning(f"AUTHENTICATION DISABLED! ALLOWING EVERYBODY ({username})")
            # Temporarily disable authentication so we can demo plato w/o an AD server.  This is not production code!
            if False:
                logger.info('From AD initializing')
                ldapClient = ldap.initialize(settings.AUTH_LDAP_SERVER_URI)
                # Hardcoded domain name of "ENG" will need to be changed
                ldapClient.simple_bind_s('%s\\%s' % ('YOUR_DOMAIN_NAME', username), password)
                logger.info('From AD simple bind')
                ldapClient.unbind_s()
                return True
            else:
                return True
        except AuthenticatedUser.DoesNotExist as e:
            logger.warning(f"invalid user tried to login and was denied: {username}")
            return False
        except Exception as e:
            logger.exception("Exception occurred in AD")
            return False
