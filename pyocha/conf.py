import os.path
import os

def basedir():
    conf_dir=os.environ['HOME'] + "/.ocha/"
    if not os.path.exists(conf_dir):
        os.makedirs(conf_dir)
    return conf_dir

def catalog_path():
    return os.path.join(basedir(), "catalog")

def timeout():
    return 0.8
