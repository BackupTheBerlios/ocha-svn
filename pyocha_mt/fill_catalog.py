import conf
import sys, os, os.path
from catalog import Catalog
import time
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

EXTS=[ ".c", ".h", ".py", ".java", ".desktop"]
def isinteresting(path, file):
    for ext in EXTS:
        if file.endswith(ext):
            return True
    return False

catalog=Catalog(conf.catalog_path())

xemacs=findOrCreateXEmacs(catalog)
run=findOrCreateRun(catalog)
project=findOrCreateProject(catalog)

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
    catalog.insertEntry(file, display_name, run)


def addTextEntry(file):
    catalog.insertEntry(file, command=xemacs)

def addProject(file):
    name=os.path.basename(file)
    parent=os.path.dirname(file)
    while os.path.exists(os.path.join(parent, "CVS")):
        name=os.path.join(os.path.basename(parent), name)
        parent=os.path.dirname(parent)
    if name.startswith("trunk") or file[0] in ( '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' ):
        name=os.path.join(os.path.basename(parent), name)
    print "add project: "+name+" ("+file+")"
    catalog.insertEntry(path=file, display_name=name, command=project)

for dir in args:
    for root, dirs, files in os.walk(dir):
        if isforbidden(root):
            continue
        if "project.xml" in files:
            if not catalog.entryForPath(root):
                addProject(root)

        existing=catalog.entriesInDirectory(dir)
        existing_files=map(lambda x: x.path, existing)
        for file in files:
            if isinteresting(root, file):
                file=os.path.join(root, file)
                if not file in existing_files:
                    add(file)
                    existing_files.append(file)
        for old in existing:
            if not old.exists():
                existing.delete()
        if wait:
            catalog.commit()
            time.sleep(WAIT_UNIT)

catalog.commit()


