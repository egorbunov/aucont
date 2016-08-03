import os

def script_dir_path():
    return os.path.dirname(os.path.abspath(__file__))
def test_rootfs_path():
    return os.path.join(script_dir_path(), '../rootfs')
def aucont_tool_path(tool_name):
    return os.path.join(
        script_dir_path(),
        '../aucont/bin/',
        tool_name
    )

def check(assertion, *msg):
    if not assertion:
        raise Exception(*msg)

LL_DEBUG = 5
LL_INFO = 4
LOG_LEVEL = LL_DEBUG
def debug(*args):
    if LOG_LEVEL >= LL_DEBUG:
        print('DEBUG:', *args)
def log(*args):
    if LOG_LEVEL >= LL_INFO:
        print('INFO:', *args)
