import unittest
from testutils import FileFixture
from catalog import *
import os.path
import sqlite

class CatalogFixture(FileFixture):
    def setUp(self):
        FileFixture.setUp(self)
        self.path=os.path.join(self.dir, "db")
        self.catalog=Catalog(self.path)

    def tearDown(self):
        FileFixture.tearDown(self)
        self.catalog.close()

class TestRunCommand(unittest.TestCase):
    def setUp(self):
        self.fix=CatalogFixture()
        self.fix.setUp()
        self.catalog=self.fix.catalog
        self.dir=self.fix.dir
        self.out=os.path.join(self.dir, 'out')
        out=self.out
        self.cmd=Command(catalog=None, id=1, display_name="test", execute="echo %f >"+out)
        self.entry=Entry(catalog=None, id=1, filename="hello.txt", directory="/tmp", display_name="hello.txt")

    def tearDown(self):
        self.fix.tearDown()

    def testExecuteWithEntry(self):
        out=self.out
        pid=self.cmd.executeWithEntry(self.entry)
        rpid, status=os.waitpid(pid, 0)
        self.failUnless(os.WIFEXITED(status))
        self.assertEquals(0, os.WEXITSTATUS(status))
        outh=open(out, "r")
        try:
            self.assertEquals("/tmp/hello.txt\n", outh.readline())
        finally:
            outh.close()


class TestCatalog(unittest.TestCase):
    def setUp(self):
        self.fix=CatalogFixture()
        self.fix.setUp()
        self.catalog=self.fix.catalog

    def tearDown(self):
        self.fix.tearDown()

    def testCreate(self):
        self.catalog.close()
        self.failUnless(os.path.exists(self.catalog.path))

    def testInsertCommand(self):
        catalog=self.catalog
        cmd=catalog.insertCommand("Web Browser", "firefox file://%f")
        self.failUnless(cmd.id)
        cmd2=catalog.loadCommand(cmd.id)
        self.assertEquals(cmd.catalog, cmd2.catalog)
        self.assertEquals(cmd.id, cmd2.id)
        self.assertEquals(cmd.display_name, cmd2.display_name)
        self.assertEquals(cmd.execute, cmd2.execute)

    def testInsertEntry(self):
        catalog=self.catalog
        entry=catalog.insertEntry("/home/zermatten/tmp/hello.txt")
        self.failUnless(entry.id)
        self.assertEquals("/home/zermatten/tmp", entry.directory)
        self.assertEquals("hello.txt", entry.filename)
        self.assertEquals("/home/zermatten/tmp/hello.txt", entry.path)
        self.assertEquals("hello.txt", entry.display_name)
        self.assertEquals(None, entry.command_id)

    def testInsertEntryWithCommand(self):
        catalog=self.catalog
        cmd=catalog.insertCommand("Web Browser", "firefox file://%f")
        entry=catalog.insertEntry("/home/zermatten/tmp/hello.txt", command=cmd)
        self.failUnless(entry.id)
        self.assertEquals(cmd.id, entry.command_id)


class Collector:
    def __init__(self):
        self.entries=[]
        self.stop=None

    def stop_at(self, end):
        self.stop=end

    def callback(self, catalog, entry):
        self.entries.append(entry)
        return self.stop!=entry

class TestQuery(unittest.TestCase):
    def setUp(self):
        self.fix=CatalogFixture()
        self.fix.setUp()
        self.catalog=self.fix.catalog
        catalog=self.catalog

        self.notepad=catalog.insertCommand("Notepad", "/usr/bin/notepad %f")
        self.emacs=catalog.insertCommand("emacs", "/usr/local/bin/emacs %f")

        self.toto_c=catalog.insertEntry("/home/zermatten/projects/toto/toto.c", command=self.emacs)
        self.toto_h=catalog.insertEntry("/home/zermatten/projects/toto/toto.h", command=self.emacs)
        self.total_h=catalog.insertEntry("/home/zermatten/projects/toto/total.h", command=self.emacs)

        self.etalma_c=catalog.insertEntry("/home/zermatten/projects/toto/etalma.c", command=self.emacs)
        self.talm_c=catalog.insertEntry("/home/zermatten/projects/toto/talm.c", command=self.emacs)
        self.talm_h=catalog.insertEntry("/home/zermatten/projects/toto/talm.h", command=self.emacs)

        self.hello_txt=catalog.insertEntry("/home/zermatten/tmp/hello.txt", command=self.notepad)
        self.hullo_txt=catalog.insertEntry("/home/zermatten/tmp/hullo.txt", command=self.notepad)

    def tearDown(self):
        self.fix.tearDown()

    def query(self, querystr):
        collector=Collector()
        self.failUnless(self.catalog.query(querystr, collector.callback))
        return collector.entries

    def queryUntil(self, querystr, entry):
        collector=Collector()
        collector.stop_at(entry)
        self.failUnless(not self.catalog.query(querystr, collector.callback))
        return collector.entries

    def testExact(self):
        self.assertEquals([ self.toto_c ] , self.query("toto.c"))

    def testStartsWith(self):
        self.assertEquals([ self.toto_c, self.toto_h ], self.query("toto"))
        self.assertEquals([ self.toto_c, self.toto_h, self.total_h ], self.query("to"))

    def testContains(self):
        self.assertEquals([ self.hello_txt, self.hullo_txt ], self.query("llo"))

    def testStartsWithAndThenContains(self):
        self.assertEquals([ self.talm_c, self.talm_h, self.etalma_c ], self.query("talm"))

    def testStop(self):
        self.assertEquals([ self.toto_c, self.toto_h ], self.queryUntil("to", self.toto_h))

if __name__ == '__main__':
    unittest.main()
