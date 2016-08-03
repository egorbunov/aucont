import subprocess
import os
import sys
import itertools

import test_utils as util

# returns container pid on success
# throws on error
def start_daemonized(image_path, *cmd_and_args,
    cpu_perc=None, cont_ip=None):
    cont_start_cmd_and_args = _make_cont_start_cmd(
        False, image_path, cmd_and_args,
        cpu_perc=cpu_perc, cont_ip=cont_ip
    )
    
    output = subprocess.check_output(cont_start_cmd_and_args)
    cont_pid = output.decode('UTF-8')[:-1]
    util.log('started container', cont_pid)
    return cont_pid

# blocks until interactive session was finished
# throws on error
def start_interactive(image_path, *cmd_and_args,
    cpu_perc=None, cont_ip=None):
    cont_start_cmd_and_args = _make_cont_start_cmd(
        True, image_path, cmd_and_args,
        cpu_perc=cpu_perc, cont_ip=cont_ip
    )

    subprocess.check_call(cont_start_cmd_and_args,
        stdin=sys.stdin, stdout=sys.stdout, stderr=sys.stderr
    )

# throws on error
def stop(cont_pid, signal=15):
    signal = str(signal)
    cont_stop_cmd_and_args = [
        util.aucont_tool_path('aucont_stop'),
        cont_pid,
        signal
    ]
    util.debug(*cont_stop_cmd_and_args)
    subprocess.check_call(cont_stop_cmd_and_args)
    util.log('stopped container', cont_pid);

# starts container, runs command, captures output and returns it
# throws on error
def run_cmd(image_path, *cmd_and_args, cpu_perc=None, cont_ip=None):
    cont_pid = start_daemonized(image_path, '/bin/sleep', '1000000',
        cpu_perc=cpu_perc, cont_ip=cont_ip)
    output = exec_capture_output(cont_pid, *cmd_and_args)
    stop(cont_pid, 9)
    return output

# blocks until interactive session was finished
# throws on error
def exec_interactive(cont_pid, *cmd_and_args):
    cont_exec_cmd_and_args = [
        util.aucont_tool_path('aucont_exec'),
        cont_pid
    ]
    cont_exec_cmd_and_args += cmd_and_args
    util.debug(*cont_exec_cmd_and_args)

    subprocess.check_call(cont_exec_cmd_and_args,
        stdin=sys.stdin, stdout=sys.stdout, stderr=sys.stderr
    )

# blocks until interactive session was finished
# throws on error
def exec_capture_output(cont_pid, *cmd_and_args):
    cont_exec_cmd_and_args = [
        util.aucont_tool_path('aucont_exec'),
        cont_pid
    ]
    cont_exec_cmd_and_args += cmd_and_args
    util.debug(*cont_exec_cmd_and_args)

    try:
        output = subprocess.check_output(cont_exec_cmd_and_args,
            stdin=sys.stdin, stderr=subprocess.STDOUT
        )
    except subprocess.CalledProcessError as err:
        util.log(err.returncode, err.output)
        raise
    return output.decode('UTF-8')

# returns list of started and not stopped container pids
# throws on error
def clist():
    cont_list_cmd_and_args = [
        util.aucont_tool_path('aucont_list')
    ]
    output = subprocess.check_output(cont_list_cmd_and_args)
    pids = output.decode('UTF-8').split('\n')
    pids = list(filter(lambda pid: pid != '', pids))
    util.debug(pids)
    return pids

def _make_cont_start_cmd(is_interactive, image_path, cmd_and_args,
    cpu_perc=None, cont_ip=None):
    cont_start_opts_list = []
    if not is_interactive: cont_start_opts_list.append('-d')
    if cpu_perc:
        cont_start_opts_list.extend(['--cpu', str(cpu_perc)])
    if cont_ip: cont_start_opts_list.extend(['--net', cont_ip])

    cont_start_cmd_and_arg_lists = [
        [util.aucont_tool_path('aucont_start')],
        cont_start_opts_list,
        [image_path],
        cmd_and_args
    ]
    cont_start_cmd_and_args = list(
        itertools.chain.from_iterable(cont_start_cmd_and_arg_lists)
    )
    util.debug(*cont_start_cmd_and_args)
    return cont_start_cmd_and_args
