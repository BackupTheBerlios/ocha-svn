import conf
import sys, os, os.path
from catalog import Catalog
import time
WAIT_UNIT=1

def findOrCreateXEmacs(catalog):
    commands=catalog.commands()
    for command in commands:
        if command.execute.startswith("xemacs-remote"):
            return command
    return catalog.insertCommand("xemacs-remote", "xemacs-remote %f")

FORBIDDEN_DIR=[".svn", "CVS", "build"]
def isforbidden(path):
    for forbidden in FORBIDDEN_DIR:
        if path.count(forbidden)>0:
            return True
    return False

EXTS=[ ".c", ".h", ".py", ".java"]
def isinteresting(path, file):
    for ext in EXTS:
        if file.endswith(ext):
            return True
    return False

catalog=Catalog(conf.catalog_path())

xemacs=findOrCreateXEmacs(catalog)

args=sys.argv[1:]
wait=False
if "--wait"==args[0]:
    wait=True
    args=args[1:]

for dir in args:
    for root, dirs, files in os.walk(dir):
        if isforbidden(root):
            continue
        existing=catalog.entriesInDirectory(dir)
        existing_files=map(lambda x: x.path, existing)
        for file in files:
            if isinteresting(root, file):
                file=os.path.join(root, file)
                if not file in existing_files:
                    catalog.insertEntry(file, command=xemacs)
        for old in existing:
            if not old.exists():
                existing.delete()
        if wait:
            catalog.commit()
            time.sleep(WAIT_UNIT)

catalog.commit()


