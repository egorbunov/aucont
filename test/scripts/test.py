#!/usr/bin/python3

import time
import os
from urllib.request import urlopen

import test_utils as util
import aucont

def test_simple_start_stop():
    util.log('[START TEST] start daemonized container and kill it')
    cont_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/bin/sleep', '5'
    )
    util.check(aucont.clist()[0] == cont_pid)
    aucont.stop(cont_pid, 9)
    util.check(len(aucont.clist()) == 0)

def test_hostname():
    util.log("[START_TEST] check that hostname inside container ",
        "is 'container'")
    output = aucont.run_cmd(
        util.test_rootfs_path(), '/bin/hostname'
    ).strip()
    util.debug(output)
    util.check(output == 'container')

def test_fs_contents():
    util.log("[START_TEST] check file system contents inside container")
    output = aucont.run_cmd(
        util.test_rootfs_path(), '/test/fs/test.sh'
    ).strip()
    util.log("\n==== CHECK THIS OUTPUT MANUALLY ====\n",
        output, "\n")

def test_daemonization():
    util.log("""[START_TEST] check that daemonized container doesn't
        use tty""")
    cont_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/test/interactive/bin/test'
    )
    time.sleep(1)
    output = aucont.exec_capture_output(
        cont_pid, "/bin/cat", "/test/interactive/bin/result.txt"
    )
    aucont.stop(cont_pid, 9)
    output = output.strip()
    util.debug(output)
    util.check(output != 'Ok')

def test_user_is_root():
    util.log("""[START_TEST] check that user name inside container
        is root""")
    output = aucont.run_cmd(
        util.test_rootfs_path(), '/bin/sh', '-c', 'whoami'
    ).strip()
    util.check(output == 'root')

def test_user_root_is_fake():
    util.log("""[START_TEST] check that root inside container
        is not initial user namespace root""")
    aucont.run_cmd(
        util.test_rootfs_path(), '/test/kmodule/bin/test',
        '/test/kmodule/module.ko'
    )

def test_cpu_perc_limit():
    util.log("""[START_TEST] check that cpu limitation
        works ok for container""")
    output = aucont.run_cmd(
        util.test_rootfs_path(), '/test/busyloop/bin/run.sh'
    ).strip()
    unlimited_result = int(output)

    output = aucont.run_cmd(
        util.test_rootfs_path(), '/test/busyloop/bin/run.sh',
        cpu_perc=20
    ).strip()
    limited_result_20_perc = int(output)
    util.debug(unlimited_result, limited_result_20_perc)

    cpu_boost = unlimited_result / limited_result_20_perc
    util.check(cpu_boost >= 3 and cpu_boost <= 5)

def test_basic_networking():
    util.log(
        """[START TEST] start container with enabled networking and
        ping back and forth. Warning: stop network manager before
        running all network tests"""
    )
    cont_ip = '10.0.0.1'
    host_ip = '10.0.0.2'
    aucont.run_cmd(
        util.test_rootfs_path(), '/test/net/test.sh',
        cont_ip, host_ip, cont_ip=cont_ip
    )

def test_webserver():
    util.log(
        """[START TEST] start container with enabled networking,
        run web server on priveledged port inside.
        Warning: stop network manager before running all
        network tests"""
    )
    cont_ip = '192.168.1.1'
    cont_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/test/web/server.sh',
        '80', cont_ip=cont_ip
    )
    time.sleep(2)
    url = 'http://' + cont_ip + ':80/file.txt'
    http_resp = urlopen(url)
    aucont.stop(cont_pid, 9)
    util.check(http_resp.status == 200)
    http_resp_str = http_resp.read().decode('utf-8')
    util.debug(http_resp_str)
    util.check(http_resp_str.index('OK!') == 0)

def test_many_conts_start_stop():
    util.log("""[START_TEST] start 3 containers.
        Wait exiting of some of them.
        Stop all the containers manually.""")
    cont1_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/bin/sleep', '1'
    )
    cont2_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/bin/sleep', '5'
    )
    cont3_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/bin/sleep', '10'
    )
    time.sleep(4)
    aucont.stop(cont3_pid, 9)
    aucont.stop(cont1_pid, 9)
    aucont.stop(cont2_pid, 9)

def test_many_cont_list():
    util.log("""[START_TEST] start 3 containers.
        Check that aucont_list tool returns right
        values.""")
    cont1_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/bin/sleep', '1'
    )
    cont2_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/bin/sleep', '5'
    )
    cont3_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/bin/sleep', '10'
    )
    cont_list = aucont.clist()
    util.check(len(cont_list) == 3)
    cont_list.index(cont1_pid)
    cont_list.index(cont2_pid)
    cont_list.index(cont3_pid)
    cleanup()

def test_many_cont_networks():
    util.log(
        """[START TEST] start 2 containers run simple networking
        tests in each one.
        Warning: stop network manager before running all
        network tests"""
    )
    cont1_ip = '192.168.1.1'
    host1_ip = '192.168.1.2'
    cont1_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/bin/sleep', '10000',
        cont_ip=cont1_ip
    )
    
    cont2_ip = '10.0.0.1'
    host2_ip = '10.0.0.2'
    cont2_pid = aucont.start_daemonized(
        util.test_rootfs_path(), '/bin/sleep', '10000',
        cont_ip=cont2_ip
    )

    time.sleep(2)

    aucont.exec_capture_output(cont1_pid, '/test/net/test.sh',
        cont1_ip, host1_ip
    )
    aucont.exec_capture_output(cont1_pid, '/test/net/test.sh',
        cont1_ip, host1_ip
    )

    cleanup()

def test_start_with_interactive_shell():
    util.log('[START TEST] start container with interactive shell')
    aucont.start_interactive(
        util.test_rootfs_path(), '/bin/sh'
    )
    util.check(len(aucont.clist()) == 0)

def cleanup():
    aucont_init_path = util.aucont_tool_path('aucont_init')
    if (os.path.isfile(aucont_init_path)):
        os.system(aucont_init_path)
    os.system(util.aucont_tool_path('aucont_list')
        + " | xargs -I '{}' "
        + util.aucont_tool_path('aucont_stop')
        + " '{}' 9"
    )
    aucont_cleanup_path = util.aucont_tool_path('aucont_cleanup')
    if (os.path.isfile(aucont_cleanup_path)):
        os.system(aucont_cleanup_path)

def build_tools():
    aucont_build_dir_path = util.aucont_tool_path('../')
    print(aucont_build_dir_path)
    os.system('cd ' + aucont_build_dir_path + ' && ./build.sh')

def main():
    build_tools()
    cleanup()
    try:
        test_simple_start_stop()
        test_hostname()
        test_fs_contents()
        test_daemonization()
        test_user_is_root()
        test_user_root_is_fake()
        test_cpu_perc_limit()
        test_basic_networking()
        test_webserver()

        test_many_conts_start_stop()
        test_many_cont_list()
        test_many_cont_networks()

        test_start_with_interactive_shell()

        util.log('===== ALL TESTS ARE PASSED! =====')

    except:
        cleanup()
        raise

if __name__ == '__main__':
    main()
