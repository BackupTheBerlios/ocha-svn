import tempfile
import os
import os.path

def deltree(top):
    for root, dirs, files in os.walk(top, topdown=False):
        for name in files:
            os.remove(os.path.join(root, name))
        for name in dirs:
            os.rmdir(os.path.join(root, name))
    os.rmdir(top)

class FileFixture:
    def setUp(self):
        self.dir=tempfile.mkdtemp()

    def tearDown(self):
        deltree(self.dir)
