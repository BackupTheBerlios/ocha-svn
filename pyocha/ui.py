import pygtk
pygtk.require('2.0')

import gtk, gtk.glade, gobject
from catalog import Catalog
import os
import os.path
import conf
import time

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

    def getDispyoplayName(self):
        return self.display_name

    def getPath(self):
        return self.path

    def execute(self, query):
        command_id=self.result.command_id
        catalog=self.query.openCatalog()
        command=catalog.loadCommand(command_id)
        command.executeWithEntry(self.result)
        catalog.remember(query, self.result, command)

class Query:
    def __init__(self, catalog_path):
        self.__query=""
        self.__observers=[]
        self.__results=[]
        self.__catalog_path=catalog_path
        self.__catalog=None

    def stop(self):
        if self.__catalog:
            self.__catalog.close()
            self.__catalog=None

    def getQueryString(self):
        return self.__query

    def getQueryResults(self):
        return [] + self.__results

    def addResult(self, display_name, path, result):
        if os.path.exists(path):
            self.__results.append(QueryResult(self, display_name=display_name, path=path, result=result))

    def set(self, str):
        self.__query=str
        self.__sendQueryChanged()
        self.__run_query()

    def append(self, str):
        self.__query=self.__query+str
        self.__sendQueryChanged()
        self.__run_query()

    def backspace(self):
        if len(self.__query)>0:
            self.__query=self.__query[0:-1]
            self.__sendQueryChanged()
            self.__run_query()


    def reset(self):
        self.__query=""
        self.__sendQueryChanged()
        self.__stop_query()


    def openCatalog(self):
        if not self.__catalog:
            self.__catalog=Catalog(self.__catalog_path)
        return self.__catalog

    def __stop_query(self):
        self.__results=[]
        self.__sendListChanged()

    def __run_query(self):
        self.__stop_query()
        if not self.__query:
            return
        catalog=self.openCatalog()
        catalog=self.__catalog
        catalog.query(self.__query, self.__query_cb)
        self.__sendListChanged()

    def __query_cb(self, catalog, entry):
        if len(self.__results)>10:
            return False
        self.addResult(entry.display_name, entry.path, entry)
        self.__sendListChanged()
        return True

    def addObserver(self, observer):
        self.__observers.append(observer)

    def __sendListChanged(self):
        for observer in self.__observers:
            observer.listChanged(self)

    def __sendQueryChanged(self):
        for observer in self.__observers:
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

        win.connect("map_event", self.cb_map_event)

    def cb_map_event(self, event, userdata):
        self.win.present()

    def cb_focus_out_event(self, event, userdata):
        self.win.hide()

    def show(self):
        self.win.show()

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
            self.selectedResult.execute(self.query.getQueryString())

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

query=Query(conf.catalog_path())
win=QueryWin(query, conf.timeout())

import signal
def sighndl(sig, stack):
    win.show()
signal.signal(signal.SIGUSR1, sighndl)


if __name__ == '__main__':
    win.show()
    gtk.main()
