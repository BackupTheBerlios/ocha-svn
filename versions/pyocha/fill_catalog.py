import conf
import sys, os, os.path
from catalog import Catalog
import time
from HTMLParser import HTMLParser
from xdg.DesktopEntry import DesktopEntry
WAIT_UNIT=1

def findOrCreateXEmacs(catalog):
    commands=catalog.commands()
    for command in commands:
        if command.execute.startswith("xemacs-remote"):
            return command
    return catalog.insertCommand("xemacs-remote", "xemacs-remote %f")


def findOrCreateRun(catalog):
    commands=catalog.commands()
    for command in commands:
        if command.execute.startswith("run"):
            return command
    return catalog.insertCommand("run", "run-desktop-entry %f")

def findOrCreateUrl(catalog):
    commands=catalog.commands()
    for command in commands:
        if command.execute.startswith("url"):
            return command
    return catalog.insertCommand("url", "gnome-moz-remote %f")

def findOrCreateProject(catalog):
    commands=catalog.commands()
    for command in commands:
        if command.execute.startswith("project"):
            return command
    return catalog.insertCommand("project", "project %f")


FORBIDDEN_DIR=[".svn", "CVS", "build"]
def isforbidden(path):
    for forbidden in FORBIDDEN_DIR:
        if path.count(forbidden)>0:
            return True
    return False

EXTS=[ ".c", ".h", ".py", ".java", ".desktop", ".txt", ".xml", ".xsl", ".txt", ".jsp", ".inc", ".jspf" ]
def isinteresting(path, file):
    for ext in EXTS:
        if file.endswith(ext):
            return True
    return False

catalog=Catalog(conf.catalog_path())

xemacs=findOrCreateXEmacs(catalog)
run=findOrCreateRun(catalog)
project=findOrCreateProject(catalog)
url_cmd=findOrCreateUrl(catalog)

args=sys.argv[1:]
wait=False
if "--wait"==args[0]:
    wait=True
    args=args[1:]

def add(file):
    if file.endswith(".desktop"):
        addDesktopEntry(file)
    else:
        addTextEntry(file)

def addDesktopEntry(file):
    entry=DesktopEntry()
    try:
        entry.parse(file)
    except:
        return
    if entry.getNoDisplay():
        return
    tryexec=entry.getTryExec()
    if tryexec and not os.path.exists(tryexec):
        return

    entry.setLocale("C")
    display_name=entry.getName()
    if not display_name:
        display_name=os.path.basename(file)[0:-len(".desktop")]
    catalog.insertEntry(file, display_name, run)


def addTextEntry(file):
    catalog.insertEntry(file, command=xemacs)

def addProject(projectbase, path):
    projectname=path[len(projectbase):]
    if projectname.startswith("/"):
        projectname=projectname[1:]
    catalog.insertEntry(path=path,
                        display_name=projectname,
                        command=project)

def extractUrls(htmlfile):
    class MyHTMLParser(HTMLParser):
        def __init__(self):
            HTMLParser.__init__(self)
            self.urls=[]
            self.current_href=None
            self.current_label=None

        def handle_starttag(self, tag, attrs):
            if "a"==tag:
                for name, value in attrs:
                    if "href"==name:
                        self.current_href=value
                        self.current_label=""

        def handle_endtag(self, tag):
            if "a"==tag :
                if self.current_href and self.current_label:
                    self.urls.append( ( self.current_href, self.current_label ) )
                self.current_href=None
                self.current_label=None

        def handle_data(self, data):
            if self.current_href:
                self.current_label+=data

    parser=MyHTMLParser()
    parser.feed(open(htmlfile).read())
    parser.close()

    print str(parser.urls)

    return parser.urls

PROJECTPATH=[ "/work/cvsvap",
              "/work/cvsapp",
              "/work/blossom",
              "/work/cvsoag",
              "/opt/projects",
              os.environ["HOME"]+"/projects" ]
SPECIAL=[ "CVS", ".svn" ]

for dir in args:
    if os.path.isfile(dir) and dir.endswith(".html"):
        for url, label in extractUrls(dir):
            print "insert entry; %s %s" % (url, label)
            catalog.insertEntry(path=url,
                                display_name=label,
                                command=url_cmd)
        catalog.commit()
    else:
      for root, dirs, files in os.walk(dir):
        if isforbidden(root):
            continue
        for projectpath in PROJECTPATH:
            if root.startswith(projectpath):
                for dir in dirs:
                    for special in SPECIAL:
                        if not special in dirs and os.path.exists(os.path.join(root, dir, special)):
                            addProject(projectpath, os.path.join(root, dir))
        for file in files:
            if isinteresting(root, file):
                file=os.path.join(root, file)
                add(file)
        catalog.commit()
        if wait:
            time.sleep(WAIT_UNIT)

catalog.removeStaleEntries()

catalog.commit()


