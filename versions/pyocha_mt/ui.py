import pygtk
pygtk.require('2.0')

import gtk, gtk.glade, gobject
from catalog import Catalog
import os
import os.path
import conf
import time
import threading

def now():
    return time.time()

class QueryObserver:
    def queryChanged(self, query):
        pass

    def listChanged(self, query):
        pass

class QueryResult:
    last_id=0
    def __init__(self, query, display_name, result, path=None):
        self.display_name=display_name
        QueryResult.last_id=QueryResult.last_id+1
        self.id=QueryResult.last_id
        self.result=result
        self.path=path
        self.query=query

    def getDisplayName(self):
        return self.display_name

    def getPath(self):
        return self.path

    def execute(self):
        command_id=self.result.command_id
        catalog=self.query.openCatalog()
        try:
            command=catalog.loadCommand(command_id)
            command.executeWithEntry(self.result)
        finally:
            catalog.close()


class QueryThread(threading.Thread):
    def __init__(self, catalog_path, catalog_cb, query_end_cb, condition):
        threading.Thread.__init__(self)
        self.path=catalog_path
        self.query=""
        self.catalog_cb=catalog_cb
        self.query_end_cb=query_end_cb
        self.condition=condition

    def run(self):
        condition=self.condition
        path=self.path
        end_cb=self.query_end_cb
        while(True):

            condition.acquire()
            try:
                while not self.query:
                    condition.wait()
                q=self.query
                self.query=""
            finally:
                condition.release()

            def cb(catalog, entry):
                return self.catalog_cb(q, catalog, entry)

            catalog=Catalog(path)
            try:
                print "run query: "+q
                catalog.query(q, cb)
                end_cb(q)
            finally:
                catalog.close()


class Query:
    def __init__(self, catalog_path):
        self.path=catalog_path
        # threading: query, runquery and result protected by 'condition'
        self.__results=[]
        self.__query=""
        self.__runquery=False
        self.__condition=threading.Condition()
        c=self.__condition

        # threading: __observers protected by 'observers_lock'
        self.__observers=[]
        self.__observers_lock=self.__condition

        self.qthread=QueryThread(catalog_path,
                                 self.__query_cb,
                                 self.__query_end_cb,
                                 self.__condition)
        self.qthread.setDaemon(True)
        self.qthread.start()

    def __query_cb(self, querystr, catalog, entry):
        self.__condition.acquire()
        try:
            if not self.__runquery or self.__query!=querystr:
                return False
            if len(self.__results)>10:
                return False
            print "result "+str(len(self.__results))+" for "+self.__query
            self.__results.append(QueryResult(entry.display_name,
                                              entry.path,
                                              entry))
        finally:
            self.__condition.release()
        self.__sendListChanged()
        return True

    def __query_end_cb(self, querystr):
        self.__condition.acquire()
        try:
            if self.__query!=querystr:
                return
        finally:
            self.__condition.release()

        self.__sendListChanged()

    def stop(self):
        """Stop a running query, even if not
        all results were found"""
        self.__condition.acquire()
        try:
            self.__runquery=False
        finally:
            self.__condition.release()

    def getQueryString(self):
        """Get the current query string"""
        return self.__query

    def getQueryResults(self):
        """Get the current query results.

        @return a new list that can be modified safely
        """
        self.__condition.acquire()
        try:
            return [] + self.__results
        finally:
            self.__condition.release()

    def set(self, str):
        """Set the query string"""
        print "set to: "+str
        if self.__query==str:
            return
        self.__condition.acquire()
        try:
            self.__query=str
            self.__results=[]
            if not self.__query:
                self.__runquery=False
                return
            self.__runquery=True
            self.qthread.query=self.__query
            self.__condition.notifyAll()
        finally:
            self.__condition.release()
        self.__sendQueryChanged()

    def append(self, str):
        """Append something to the query string"""
        self.set(self.getQueryString()+str)

    def backspace(self):
        """Remove one character from the query string"""
        str=self.getQueryString()
        if len(str) >= 1:
            self.set(str[0:-1])

    def reset(self):
        """Reset the query string"""
        self.set("")

    def openCatalog(self):
        """Get a new catalog object, opening it if necessary.

        The caller is responsible for closing the catalog.
        """
        return Catalog(self.path)

    def addObserver(self, observer):
        self.__observers_lock.acquire()
        try:
            self.__observers.append(observer)
        finally:
            self.__observers_lock.release()

    def __getObservers(self):
        self.__observers_lock.acquire()
        try:
            return [] + self.__observers
        finally:
            self.__observers_lock.release()

    def __sendListChanged(self):
        for observer in self.__getObservers():
            observer.listChanged(self)

    def __sendQueryChanged(self):
        for observer in self.__getObservers():
            observer.queryChanged(self)

class QueryWin(QueryObserver):
    def __init__(self, query, timeout):
        self.timeout=timeout
        self.query=query
        self.__results={}
        xml = gtk.glade.XML('pyocha.glade')
        self.win=xml.get_widget("querywin")
        self.query_widget=xml.get_widget("query")
        self.list_widget=xml.get_widget("list")
        self.__initlist()
        xml.signal_autoconnect(self)
        self.queryChanged(query)
        self.listChanged(query)
        self.win.connect("key_release_event", self.cb_key_release_event)

        query.addObserver(self)

        win=self.win
        win.stick()
        win.set_keep_above(True)
        win.connect("focus_out_event", self.cb_focus_out_event)

    def cb_focus_out_event(self, event, userdata):
        self.win.hide()

    def show(self):
        self.win.present()

    def ping(self):
        self.__ping=now()

    def __initlist(self):
        self.list_model=gtk.ListStore(gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING)
        view=self.list_widget

        namecol=gtk.TreeViewColumn("Label", gtk.CellRendererText(), text=0)
        view.append_column(namecol)

        pathcol=gtk.TreeViewColumn("Path", gtk.CellRendererText(), text=1)
        view.append_column(pathcol)

        objcol=gtk.TreeViewColumn("Object", gtk.CellRendererText())
        view.append_column(objcol)

        view.set_model(self.list_model)
        view.get_selection().connect("changed", self.cb_select)

    def queryChanged(self, query):
        self.query_widget.set_text(query.getQueryString())
        self.ping()

    def listChanged(self, query):

        self.ping()
        model=self.list_model
        iter=model.get_iter_first()
        results=query.getQueryResults()
        print "refresh list: "+str(len(results))
        for result in results:
            if not iter:
                iter=model.append()
            model.set_value(iter, 0, result.getDisplayName())
            model.set_value(iter, 1, result.getPath())
            model.set_value(iter, 2, result.id)
            self.__results[str(result.id)]=result
            iter=model.iter_next(iter)
        while iter:
            next=model.iter_next(iter)
            model.remove(iter)
            iter=next

        selection=self.list_widget.get_selection()
        selected=selection.get_selected()
        first=model.get_iter_first()
        if first:
            selection.select_iter(first)
            self.selectedResult=results[0]

    def execute(self):
        if self.selectedResult:
            self.selectedResult.execute()

    def findResult(self, id):
        return self.__results[id]

    def cb_select(self, treeselection):
        model, iter = treeselection.get_selected()
        if iter:
            result_id=self.list_model.get_value(iter, 2)
            result=self.findResult(result_id)
            self.selectedResult=result
        else:
            self.selectedResult=None

    def stop(self):
        self.win.hide()
        self.query.stop()
        return

    def cb_key_release_event(self, widget, event):
        if gtk.keysyms.Escape==event.keyval:
            self.stop()
        elif event.keyval in ( gtk.keysyms.Delete, gtk.keysyms.BackSpace):
            self.query.backspace()
        elif event.keyval==gtk.keysyms.Return:
            self.execute()
            self.stop()
        elif event.string:
            if (now()-self.__ping)>self.timeout:
                self.query.set(event.string)
            else:
                self.query.append(event.string)

gtk.gdk.threads_init()

query=Query(conf.catalog_path())
win=QueryWin(query, conf.timeout())

import signal
def sighndl(sig, stack):
    win.show()
signal.signal(signal.SIGUSR1, sighndl)


if __name__ == '__main__':
    win.show()
    gtk.main()
