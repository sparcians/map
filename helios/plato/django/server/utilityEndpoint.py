import json
from logging import debug, info, exception
import math
import sys
import time
import traceback

from channels.generic.websocket import AsyncWebsocketConsumer
from django.core.exceptions import MultipleObjectsReturned
from django.db import IntegrityError

from .models import Layout


class utilityEndpoint(AsyncWebsocketConsumer):
    '''
    This is a websocket endpoint for various utilities that are used by plato.
    the ability to load, save, autosave, etc. are all contained in here
    '''

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    async def connect(self):
        '''
        look at the call that this wants to use and cache that
        will handle <host>:<port>//ws/utility for now
        '''
        await self.accept()

    async def disconnect(self, closeCode):
        '''
        upon disconnect from the client
        '''
        await self.close()

    async def receive(self, text_data):
        '''
        run utility commands
        '''
        start = time.time()
        # prepare a return value that's anticipating an error
        returnValue = {"reqSeqNum": 0,
                       "command": "",
                       "result": "error",
                       "error": "unknown"}
        try:
            jsonData = json.loads(text_data)
            command = returnValue["command"] = jsonData['command']
            returnValue["reqSeqNum"] = jsonData["reqSeqNum"]
            debug(f"ws/utility() {command}")
            if command == "saveLayout":
                await self.saveLayout(jsonData, returnValue)
            elif command == "loadLayout":
                await self.loadLayout(jsonData, returnValue)
            elif command == 'getAllLayouts':
                await self.getAllLayouts(jsonData, returnValue)
            elif command == 'deleteLayout':
                await self.deleteLayout(jsonData, returnValue)
            else:
                raise ValueError(f"unknown command: {command}")
        except Exception as e:
            exception("error in sources()")
            returnValue["error"] = str(e)
            await self.send(text_data = json.dumps(returnValue))
        debug("utility() took {}ms".format((time.time() - start) * 1000))

    async def saveLayout(self, jsonData, returnValue):
        '''
        save the layout to the database
        '''
        start = time.time()

        try:
            # TODO make this mandatory in the future
            user = jsonData.get("user", "unknown")
            layoutName = jsonData["name"]
            overwrite = bool(jsonData.get("overwrite", False))
            content = jsonData["content"]
            content = json.dumps(json.loads(content))
            version = jsonData["version"]

            if overwrite:
                try:
                    obj, _ = Layout.objects.update_or_create(uid = user,
                                                             layoutName = layoutName,
                                                             defaults = {"layout": content,
                                                                         "version": version})
                    if obj:
                        returnValue["result"] = "success"
                        returnValue["error"] = ""
                except IntegrityError as e:
                    returnValue["error"] = f"not created, {str(e)}"
            else:
                try:
                    _ = Layout.objects.create(uid = user,
                                              layoutName = layoutName,
                                              layout = content,
                                              version = version)
                    returnValue["result"] = "success"
                    returnValue["error"] = ""
                except IntegrityError as e:
                    returnValue["error"] = f"not created, layout already exists, did you mean to overwrite?"
        except KeyError as ke:
            returnValue["error"] = str(ke)

        await self.send(text_data = json.dumps(returnValue))

        debug("saveLayout() took {}ms".format((time.time() - start) * 1000))

    async def loadLayout(self, jsonData, returnValue):
        '''
        load and return the layout
        '''
        start = time.time()

        try:
            # TODO make this mandatory in the future
            user = jsonData.get("user", "unknown")
            layoutName = jsonData["name"]

            try:
                obj = Layout.objects.get(uid = user,
                                         layoutName = layoutName)
                returnValue["result"] = "success"
                returnValue["error"] = ""
                returnValue["content"] = obj.layout
                returnValue["version"] = obj.version
                returnValue["updateTime"] = str(obj.updateTime)
            except (Layout.DoesNotExist, MultipleObjectsReturned) as e:
                returnValue["error"] = f"not created, {str(e)}"
        except KeyError as ke:
            returnValue["result"] = "error"
            returnValue["error"] = str(ke)

        debug(f"sending {json.dumps(returnValue)}")
        await self.send(text_data = json.dumps(returnValue))

        debug("loadLayout() took {}ms".format((time.time() - start) * 1000))

    async def deleteLayout(self, jsonData, returnValue):
        '''
        delete the layout for a given user
        '''
        start = time.time()

        try:
            user = jsonData.get("user")
            layoutName = jsonData["name"]

            layoutObj = Layout.objects.get(uid = user,
                                           layoutName = layoutName)
            layoutObj.delete()
            returnValue["result"] = "success"
            returnValue["error"] = ""
            returnValue["layoutName"] = layoutName
        except KeyError as ke:
            returnValue["result"] = "error"
            exception(f"problem in deleteLayout(), {jsonData}")
            returnValue["stackTrace"] = traceback.format_exc()
            returnValue["errorMessage"] = str(ke)
            returnValue["durationMs"] = math.ceil((time.time() - start) * 1000)

        await self.send(text_data = json.dumps(returnValue))

        debug(f"deleteLayout() took {(time.time() - start) * 1000}ms")

    async def getAllLayouts(self, jsonData, returnValue):
        '''
        get all available saved layouts for a given user
        '''
        start = time.time()

        try:
            user = jsonData.get("user", "unknown")

            kwargs = {'uid': user}
            if "version" in jsonData:
                kwargs['version'] = jsonData["version"]

            layouts = Layout.objects.all().filter(**kwargs).order_by('-updateTime')

            returnValue["layouts"] = []
            for layout in layouts:
                returnValue["layouts"].append({"name": layout.layoutName,
                                               "version": layout.version,
                                               "layout": layout.layout,
                                               "updateTime": str(layout.updateTime)})

            returnValue["error"] = ""
            returnValue["result"] = "success"

            await self.send(text_data = json.dumps(returnValue))
        except Exception as e:
            exception("problem when running getAllLayouts()")
            returnValue["stackTrace"] = traceback.format_exc()
            returnValue["errorMessage"] = str(e)
            returnValue["result"] = "error"

        debug("getAllLayouts() took {}ms".format((time.time() - start) * 1000))

