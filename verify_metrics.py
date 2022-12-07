#!/usr/bin/env python
import argparse
import glob
import logging
import os
import re
import signal
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from os import path


class Collectd:
    def __init__(self, conf, pid):
        self.pid = pid
        subprocess.check_call(
            ["/usr/sbin/collectd", "-C", conf, "-P", self.pid],
            stdout=sys.stdout.fileno(),
            stderr=sys.stderr.fileno(),
        )

    def stop(self):
        os.kill(int(open(self.pid, "r").read().strip()), signal.SIGTERM)


def parse_rrd(rrd):
    tree = ET.fromstring(
        subprocess.check_output(["rrdtool", "dump", rrd], stderr=sys.stderr.fileno())
    )

    return tree.find("./ds/last_ds").text.strip().split(".")[0]


def check_result(logger, dmission, tests):
    failures = 0
    missings = 0
    successes = 0

    for test in tests.findall("test"):
        test_name = test.find("name").text
        logger.info("Tested Field: %s", test_name)
        test_path = test.find("path").text
        test_pattern = test.find("pattern").text
        failed = False
        succeeded = False

        for rrd in glob.iglob(path.join(dmission, test_path)):
            content = parse_rrd(rrd)
            logger.debug("rrdtool file: '%s'" % rrd)
            logger.info("Collected: '%s', Expected '%s'" % (content, test_pattern))
            match = re.match(test_pattern, content)
            if match is None or match.end(0) != len(content):
                logger.info("FAIL")
                failed = True
            elif not failed:
                logger.info("PASS")
                succeeded = True

        if succeeded:
            successes += 1
        elif failed:
            failures += 1
        else:
            logger.info("MISSING: rrdtool file '%s'does not exist" % test_path)
            missings += 1

    return failures, missings, successes


def make_conf(
    testconf, conf, data, definition, root, log, interval, hostname, write_tsdb
):
    inside_filedata = False
    pattern_load_plugin = r"\s*LoadPlugin\s+write_tsdb\s*"
    pattern_plugin_start = r"\s*<Plugin\s+write_tsdb>\s*"
    pattern_plugin_end = r"\s*</Plugin>\s*"
    pattern_definition_file = r'\s*DefinitionFile\s+".+"\s*'
    pattern_log = r'\s*File\s+".+"\s*'
    pattern_interval = r"\s*Interval\s+\d+\s*"
    file = open(testconf, "w")
    for line in open(conf, "r"):
        if write_tsdb != "yes":
            if re.match(pattern_load_plugin, line):
                continue
            elif inside_filedata:
                if re.match(pattern_plugin_end, line):
                    inside_filedata = False
                continue
            elif re.match(pattern_plugin_start, line):
                inside_filedata = True
                continue
        if re.match(pattern_definition_file, line):
            file.write("""DefinitionFile "%s"\n""" % definition)
            file.write("""Rootpath "%s"\n""" % root)
            continue
        elif re.match(pattern_log, line):
            file.write("""File "%s"\n""" % log)
            continue
        elif re.match(pattern_interval, line):
            file.write("""Interval %d\n""" % interval)
            continue
        else:
            file.write(line)
    file.write("""Hostname "%s"\n""" % hostname)
    file.write(
        """LoadPlugin rrdtool
<Plugin rrdtool>
Datadir "%s"
</Plugin>\n"""
        % data,
    )
    file.close()


def get_log_level(level_string):
    if level_string == "DEBUG":
        level = logging.DEBUG
    elif level_string == "INFO":
        level = logging.INFO
    elif level_string == "WARNING":
        level = logging.WARNING
    elif level_string == "ERROR":
        level = logging.ERROR
    elif level_string == "CRITICAL":
        level = logging.CRITICAL
    else:
        level = logging.INFO
    return level


def run():
    def receive_signal(signum, stack):
        collectd.stop()

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--log_level",
        "-l",
        choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"],
        default="INFO",
        help="log level",
    )
    parser.add_argument(
        "--directory",
        "-d",
        default="/",
        help="working directory where pseduo stats directory is located",
    )
    parser.add_argument(
        "--config",
        "-c",
        default="/etc/collectd.conf",
        help="collectd configuration file",
    )
    parser.add_argument(
        "--definition_file",
        "-f",
        default="/etc/lustre.xml",
        help="Lustre Definition file for collectd",
    )
    parser.add_argument(
        "--test_file",
        "-t",
        default="./tests.xml",
        help="test cases for verification",
    )
    parser.add_argument(
        "--interval",
        "-i",
        type=int,
        default=1,
        help="collectd collection interval",
    )
    parser.add_argument(
        "--hostname",
        "-n",
        default="collection",
        help="hostname for collection",
    )
    parser.add_argument(
        "--write_tsdb",
        "-w",
        default="no",
        help="write data into opentsdb",
    )
    args = parser.parse_args()
    level = get_log_level(args.log_level)
    definition = args.definition_file
    conf = args.config
    content = args.directory
    tests = ET.parse(args.test_file).getroot()
    interval = args.interval
    hostname = args.hostname
    write_tsdb = args.write_tsdb
    dtemp = tempfile.mkdtemp()
    log = path.join(dtemp, "collectd.log")
    logging.basicConfig(filename=path.join(dtemp, "verification.log"))
    logger = logging.getLogger()
    logger.addHandler(logging.StreamHandler())
    logger.setLevel(level)
    logger.info("working in %s", dtemp)

    pid = path.join(dtemp, "collectd.pid")
    test_conf = path.join(dtemp, "collectd.conf")

    make_conf(
        test_conf,
        conf,
        dtemp,
        path.abspath(definition),
        path.abspath(content),
        log,
        interval,
        hostname,
        write_tsdb,
    )

    signal.signal(signal.SIGINT, receive_signal)
    signal.signal(signal.SIGTERM, receive_signal)

    collectd = Collectd(test_conf, pid)

    time.sleep(interval + 1)
    collectd.stop()

    failures, missings, successes = check_result(
        logger, path.join(dtemp, hostname), tests
    )

    logger.info(
        "total %d, failed: %d, missing: %d, success: %d"
        % (failures + missings + successes, failures, missings, successes)
    )
    if failures or missings:
        sys.exit(1)


run()
