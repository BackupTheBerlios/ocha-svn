import pygtk
pygtk.require('2.0')

import gtk, gtk.glade, gobject
from catalog import Catalog
import os
import os.path
import conf

class QueryObserver:
    def queryChanged(self, query):
        pass

    def listChanged(self, query):
        pass

class QueryResult:
    last_id=0
    def __init__(self, display_name, result, path=None):
        self.display_name=display_name
        QueryResult.last_id=QueryResult.last_id+1
        self.id=QueryResult.last_id
        self.result=result
        self.path=path

    def getDisplayName(self):
        return self.display_name

    def getPath(self):
        return self.path

    def execute(self):
        command_id=self.result.command_id
        catalog=self.result.catalog
        command=catalog.loadCommand(command_id)
        command.executeWithEntry(self.result)

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
            print "disconnect"

    def getQueryString(self):
        return self.__query

    def getQueryResults(self):
        return [] + self.__results

    def addResult(self, display_name, path, result):
        self.__results.append(QueryResult(display_name=display_name, path=path, result=result))

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
        self.sendQueryChanged()
        self.__stop_query()

    def __stop_query(self):
        pass

    def __run_query(self):
        if not self.__query:
            return
        if not self.__catalog:
            print "connect"
            self.__catalog=Catalog(self.__catalog_path)
        self.__results=[]
        self.__sendQueryChanged()
        catalog=self.__catalog
        catalog.query(self.__query, self.__query_cb)

    def __query_cb(self, catalog, entry):
        print "result:"+str(entry)
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
    def __init__(self, query):
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

    def listChanged(self, query):
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
        if iter:
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
        print "execute:"+str(self.selectedResult)
        if self.selectedResult:
            self.selectedResult.execute()

    def findResult(self, id):
        return self.__results[id]

    def cb_select(self, treeselection):
        model, iter = treeselection.get_selected()
        result_id=self.list_model.get_value(iter, 2)
        result=self.findResult(result_id)
        print "result:"+str(result)
        self.selectedResult=result

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
            self.query.append(event.string)

query=Query(conf.catalog_path())
win=QueryWin(query)
gtk.main()
