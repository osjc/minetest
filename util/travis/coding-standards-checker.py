#!/usr/bin/env python2
#
# ===- coding-standards-checker.py - Coding standards checker -*- python -*--===#
#
#                     The Minetest Coding Standards Checker
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
# ===------------------------------------------------------------------------===#
# FIXME: Integrate with clang-tidy-diff.py

"""
Coding standards checker
========================

Scans the provided compiler command line, digs out the name of the source
code file and possibly other useful information and performs a battery of
coding standards checks. Returns success if the check is successful, failure
if problems were found and reported.

Example invocations.
"""

from __future__ import print_function
import sys
import argparse


def parse_arguments(argv):
    # Program arguments parser. Used for the "options" part of the command
    # line only.
    parser = argparse.ArgumentParser(description=(
        'Digs the name of the source code file out of the compiler command line'
        'and checks the coding standards in that file'
    ))

    parser.add_argument('delim', metavar="--", nargs=1,
        help=(
            'separates the compiler command line from the options'
        )
    )
    parser.add_argument('cargs', metavar="cargs", nargs=1,
        help=(
            'compiler command line to be scanned'
        )
    )

    parser.add_argument('--cplusplus', action='store_true',
        help='Assume the sources are in C++'
    )


    # find "--" and cut the arguments into the stuff before "--" and after
    # "--". Most of the information from the compiler command line is going
    # to be ignored so. Signal an error if the compiler command line is empty
    # or there is no "--" on the line.
    error=None
    i = 0;
    while i<len(argv) and argv[i]!="--":
        i+=1
    if i<len(argv):
        command=argv[i+1:]
        if len(command)>0:
          argv=argv[:i+1]+["comp"]
        else:
          error="compiler command line empty"
    else:
      error="no compiler command line"

    # If no errors were detected above, run the arguments parser on the
    # options, otherwise run the argument parser.
    if error is not None:
        parser.error(error)
    else:
        return parser.parse_args(argv),command


def main(args):
    args,command = parse_arguments(args)


if __name__ == '__main__':
    main(sys.argv)
