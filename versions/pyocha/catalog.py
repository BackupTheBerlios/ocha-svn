import sqlite
import os.path
import os
import string
import sys
import time

SHELL="/bin/sh"

class Entry:
    """A catalog entry"""

    def __init__(self, catalog, id, filename, directory, display_name, command_id=None):
        self.catalog=catalog
        self.id=id
        self.filename=filename
        self.directory=directory
        self.path=os.path.join(self.directory, self.filename)
        self.display_name=display_name
        self.command_id=command_id

    def exists(self):
        return os.path.exists(self.path)

    def delete(self):
        catalog.removeEntry(self.id)

    def __repr__(self):
        return "entry "+str(self.id)+"="+self.path

    def __cmp__(self, other):
        if not other:
            return 1
        return self.id-other.id

class Command:
    """A command entry in the catalog"""
    def __init__(self, catalog, id, display_name, execute):
        self.catalog=catalog
        self.id=id
        self.display_name=display_name
        self.execute=execute

    def executeWithEntry(self, entry):
        pid=os.fork()
        if pid==0:
            # child
            cmdline=string.replace(self.execute, "%f", entry.path)
            os.chdir(entry.directory)
            os.execl(SHELL, SHELL, "-c", cmdline)
            sys.stderr.write("error: command failed: '"+cmdline+"'")
            sys.exit(99)
        else:
            return pid

    def __str__(self):
        return "command "+str(self.id)+"="+self.display_name


def now():
    return long(time.time()*1000)

class Catalog:
    """sqllite-based catalog"""
    def __init__(self, path):
        """Open the catalog"""
        self.path=path
        createdb=not os.path.exists(path)
        self.__c=sqlite.connect(path, encoding="utf-8", mode=0700)
        if createdb:
            self.__createdb()
        self.__query_base="SELECT a.id,a.filename,a.directory,a.display_name,a.command_id FROM entries a"
        self.__like_query=self.__query_base+" WHERE display_name LIKE %s ORDER BY lastuse DESC, id ASC"
        self.__history_query=self.__query_base+", history h WHERE h.query=%s and h.entry_id=a.id ORDER BY h.frequence DESC"
        self.__loadcommand_query="SELECT id,display_name,execute FROM command WHERE id=%s"
        self.__cursor=self.__c.cursor()
        self.__closed=False

    def dumpHistory(self):
        self.__cursor.execute("SELECT * FROM history")
        for row in self.__cursor:
            for element in row:
                sys.stdout.write(str(element)+" ")
            sys.stdout.write("\n")

    def commit(self):
        self.__c.commit()

    def rollback(self):
        self.__c.rollback()

    def loadCommand(self, id):
        """Load a command, given its id or return None"""
        cursor=self.__cursor
        cursor.execute(self.__loadcommand_query, ( id ))
        row=cursor.fetchone()
        if not row:
            return None
        return Command(catalog=self, id=row[0], display_name=row[1], execute=row[2])

    def removeEntry(self, id):
        self.__cursor.execute("DELETE FROM entries WHERE id=%s", id)

    def removeEntryByPath(self, path):
        self.__cursor.execute("DELETE FROM entries WHERE path=%s", id)

    def commands(self):
        self.__cursor.execute("SELECT id,display_name,execute FROM command")
        retval=[]
        for row in self.__cursor:
            retval.append(Command(catalog=self, id=row[0], display_name=row[1], execute=row[2]))
        return retval


    def insertEntry(self, path, display_name=None, command=None):
        command_id=None
        if command:
            command_id=command.id

        filename=os.path.basename(path)
        directory=os.path.dirname(path)
        if not display_name:
            display_name=filename
        self.__cursor.execute("DELETE FROM entries WHERE path=%s", path)
        self.__cursor.execute("INSERT INTO entries ( id, filename, directory, path, display_name, command_id ) VALUES ( NULL, %s, %s, %s, %s, %s)",
                              (filename, directory, path, display_name, command_id))
        entry=Entry(self,
                    id=self.__cursor.lastrowid,
                    filename=filename,
                    directory=directory,
                    display_name=display_name,
                    command_id=command_id)
        return entry

    def insertCommand(self, display_name, execute):
        self.__cursor.execute("INSERT INTO command ( id, display_name, execute ) VALUES ( NULL, %s, %s )", ( display_name, execute ) )

        command=Command(self,
                        id=self.__cursor.lastrowid,
                        display_name=display_name,
                        execute=execute)
        return command

    def __escape_query(self, query):
        return query

    def entryForPath(self, path):
        """find an entry with the given path"""
        cursor=self.__cursor
        cursor.execute(self.__query_base+" WHERE path=%s", path)
        retval=None
        def collect(catalog, entry):
            retval=entry
            return False
        self.__parse_results(cursor, collect)
        return retval

    def remember(self, query, entry, command):
        """Remember that for the given query the command with the
        given entry"""
        cursor=self.__cursor
        cursor.execute("UPDATE history SET frequence=frequence*0.8 where query=%s",
                       query)
        cursor.execute("SELECT frequence FROM history WHERE "
                       +"entry_id=%s and command_id=%s and query=%s",
                       entry.id, command.id, query)
        row=cursor.fetchone()
        if row:
            cursor.execute("UPDATE history SET frequence=frequence+1.0, lastuse=%s WHERE "
                           +"entry_id=%s and command_id=%s and query=%s",
                           now(), entry.id, command.id, query)

        else:
            cursor.execute("INSERT INTO history ( entry_id, command_id, query, frequence, lastuse ) "
                           +" VALUES (%s, %s, %s, %s, %s)",
                           entry.id, command.id, query, 1.0, now())
        self.__c.commit()


    def removeStaleEntries(self):
        """go through all the entries and remove those with no corresponding file"""
        cursor=self.__cursor
        todelete=[]
        cursor.execute("SELECT path FROM entries")
        for row in cursor:
            path=row[0]
            if not os.path.exists(path):
                todelete.add(path)
        for path in todelete:
            cursor.execute("DELETE FROM entries WHERE path=%s", path)


    def query(self, query, callback):
        """execute a query and call callback on all the results, in
        the correct order.

        query: the query string
        callback: a method that takes a catalog and an entry and returns True (continue) or False (stop)

        Return True if all results were found, False if the query
        was interrupted using stop()"""

        query=self.__escape_query(query)
        cursor=self.__c.cursor()
        try:
            sofar=[]
            cursor.execute(self.__history_query, query)
            ret=self.__parse_results(cursor,callback, sofar=sofar)
            if not ret:
                return False

            cursor.execute(self.__like_query, (query+"%"))
            ret=self.__parse_results(cursor,callback, sofar=sofar)
            if not ret:
                return False

            cursor.execute(self.__like_query, ("%"+query+"%"))
            ret=self.__parse_results(cursor,callback, sofar=sofar)
            return ret
        finally:
            cursor.close()

    def __parse_results(self, cursor, callback, sofar=None):
        """Call callback on all the results, in
        the correct order.

        Return True if all results were found, False if the query
        was interrupted using a return value from a callback"""

        for row in cursor:
            id=row[0]
            if sofar==None or id not in sofar:
                if sofar!=None:
                    sofar.append(id)
                entry=Entry(self, id, filename=row[1], directory=row[2], display_name=row[3], command_id=row[4])
                if not callback(self, entry):
                    return False
        return True

    def __createdb(self):
        """Create the database; this is done the first time only"""
        cursor=self.__c.cursor()
        try:
            cursor.execute("CREATE TABLE entries (id INTEGER PRIMARY KEY, "
                                                  +"filename VARCHAR NOT NULL, "
                                                  +"directory VARCHAR, "
                                                  +"path VARCHAR UNIQUE NOT NULL, "
                                                  +"display_name VARCHAR NOT NULL, "
                                                  +"command_id INTEGER, "
                                                  +"lastuse TIMESTAMP)")
            cursor.execute("CREATE TABLE command (id INTEGER PRIMARY KEY , "
                                                  +"display_name VARCHAR NOT NULL, "
                                                  +"execute VARCHAR NOT NULL, "
                                                  +"lastuse TIMESTAMP)")
            cursor.execute("CREATE TABLE history (query VARCHAR NOT NULL, "
                                                  +"entry_id INTEGER NOT NULL, "
                                                  +"command_id INTEGER NOT NULL, "
                                                  +"lastuse TIMESTAMP NOT NULL, "
                                                  +"frequence FLOAT NOT NULL)")
            self.__c.commit()
        finally:
            cursor.close()


    def close(self):
        """Close the catalog.

        After this call, the catalog cannot be used any more."""
        if not self.__closed:
            self.__closed=True
            self.__cursor.close()
            self.__c.close()




