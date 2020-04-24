from django.db import models


class DataId(models.Model):
    '''
    a pair of file path and UUID
    '''
    path = models.CharField(max_length = 4096)
    uuid = models.CharField(max_length = 128)

    class Meta:
        constraints = [models.UniqueConstraint(fields = ['path', 'uuid'],
                                               name = 'uniqueUUIDPath')]

    def __str__(self):
        return str((self.path, self.uuid))


class Layout(models.Model):
    '''
    a per-user store for layout data, sort of general purpose
    '''
    uid = models.CharField(max_length = 64)
    layoutName = models.CharField(max_length = 128)
    layout = models.CharField(max_length = 16 * 1024)
    version = models.IntegerField(default = 0)
    updateTime = models.DateTimeField(auto_now = True)

    class Meta:
        constraints = [models.UniqueConstraint(fields = ['uid', 'layoutName'],
                                               name = 'primaryKey')]

    def __str__(self):
        return f"uid={self.uid}, layoutName={self.layoutName}, layout={self.layout}, updated={str(self.updateTime)}"


class ProcessorId(models.Model):
    '''
    a unique tuple of a dataId and processor type
    '''
    uuid = models.CharField(max_length = 128)
    dataUuid = models.CharField(max_length = 128)
    processor = models.CharField(max_length = 128)
    kwargs = models.CharField(max_length = 512)

    class Meta:
        constraints = [models.UniqueConstraint(fields = ['processor', 'dataUuid', 'kwargs'],
                                               name = 'uniqueUUIDProcessor')]

    def __str__(self):
        return f"proc={self.processor}, uuid={self.uuid}, dataUuid={self.dataUuid}, kwargs={self.kwargs}"


class LongFunctionCall(models.Model):
    '''
    a function call that took a while, log them in an easy-to-read format
    '''
    functionName = models.CharField(max_length = 8192)
    args = models.CharField(max_length = 8192)
    kwargs = models.CharField(max_length = 1024)
    execTime = models.FloatField()

    def __str__(self):
        return f"functionName={self.functionName}, time={self.execTime}, args={str(self.args)}, kwargs={str(self.kwargs)}"


class AuthenticatedUser(models.Model):
    '''
    a user who has access to the plato system
    '''
    username = models.CharField(primary_key = True, max_length = 128)
