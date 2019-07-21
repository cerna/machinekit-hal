#!/usr/bin/env python3
"""
This is HAL layer latency test for Machinekit-HAL with TUI console output
"""

__author__ = "Jakub Fišer"
__version__ = "0.0.1"
__license__ = "MIT"

import argparse
import sys
import re
from machinekit import rtapi
from machinekit import hal
from machinekit import launcher


def main(args):
    """ Main entry point of the app """
    print("hello world")
    print(args)
    print(args.period)
    prepareHAL(args.period, args.floating_point)


def prepareHAL(threadPeriods: float, fp: bool):
    sortedPeriods = sorted(threadPeriods)

    launcher.check_installation()
    launcher.cleanup_session()
    launcher.start_realtime()

    for index, period in enumerate(sortedPeriods, start=1):
        hal.newthread("lt{0}".format(index), period, fp=fp)


def cleanHAL():
    launcher.stop_realtime()


def periodTime(period: str):
    parsed = re.match(r"^([+\-]?[0-9.]+)(s|ms|μs|us|ns)?$", period)
    if parsed:
        try:
            number = float(parsed.group(1))
        except:
            msg = "Period {0} is not a valid number".format(period)
            raise argparse.ArgumentTypeError(msg)
        unit = parsed.group(2)
        if unit == 's':
            number = number * 1000000000
        elif unit == 'ms':
            number = number * 1000000
        elif unit == 'μs' or unit == 'us':
            number == number * 1000
        if number <= 5000 or number > 1000000000:
            msg = "Period {0!s}ns is not within allowed limit of smaller than 1s and larger than 4999ns".format(
                number)
            raise argparse.ArgumentTypeError(msg)
        return number
    else:
        msg = "Period %r is not in right format" % period
        raise argparse.ArgumentTypeError(msg)


if __name__ == "__main__":
    """ This is executed when run from the command line """
    parser = argparse.ArgumentParser(
        description="Latency tester for Machinekit-HAL layer")

    # Required positional argument
    parser.add_argument("period", type=periodTime, nargs="*", default=[1000000],
                        help="Periods of threads for latency test, allowed units are s, ms, μs, us and ns, number without unit are considered as in nanoseconds, default = [%(default)sns]")

    # Optional argument flag which defaults to False
    parser.add_argument("-l", "--load", action="store_true",
                        default=False, help="Generate dummy load on the system, default = %(default)s")
    # Optional argument which requires a parameter (eg. -d test)
    parser.add_argument("-fp", "--floating-point", action="store", default=True, type=bool,
                        help="Create threads with floating point support, default = %(default)s")
    
    #parser.add_argument("-n", "--name", action="store", dest="name")

    # Optional verbosity counter (eg. -v, -vv, -vvv, etc.)
    parser.add_argument(
        "-d",
        "--debug",
        action="count",
        default=0,
        help="Debug level specified by number of ds passed (-ddddd for DEBUG=5), default = %(default)s")

    # Specify output of "--version"
    parser.add_argument(
        "--version",
        action="version",
        version="%(prog)s (version {version})".format(version=__version__))

    args = parser.parse_args()
    sys.exit(main(args))
