#! /bin/env python

"""
Given snow pillow prompts on stdin, write out corresponding responses on
stdout.  Use socat to connect this to a pty which dsm can open like a serial
port:

socat PTY,link=/tmp/ttySPIUSB,raw,echo=0 EXEC:./emulate_snow_pillow.py
"""

from collections import namedtuple
import sys
import logging

logger = logging.getLogger()


Command = namedtuple('Command', ['msg', 'response'])


sequence = [
    Command('1M1!', '1001'),
    Command("1D0!", "1 1.1 2.2 3.3 4.4"),
    Command("1M!", '1002'),
    Command("1D0!", "1 5 10 15"),
    Command("1M2!", "1003"),
    Command("1D0!", "1 41 42 43 44"),
    Command("1M3!", "1004"),
    Command("1D0!", "1 1 2 3 4 5 6")
]


def find_command_index(msg):
    "Find this command in the sequence, or return None if not unique."
    ix = None
    for i, cmd in enumerate(sequence):
        if cmd.msg == msg:
            if ix is not None:
                return None
            ix = i
    return ix


def main():
    icmd = None
    logger.info("Starting to read stdin...")
    while True:
        line = sys.stdin.readline()
        msg = line.strip()
        logger.info(f"received: {msg}")
        # Either we're still on sequence, or need to sync.
        if icmd is None:
            icmd = find_command_index(msg)
        if icmd is not None and sequence[icmd].msg == msg:
            response = sequence[icmd].response
            logger.info(f"sending '{response}'")
            sys.stdout.write(response + "\n")
            sys.stdout.flush()
            icmd = (icmd + 1) % len(sequence)
        else:
            # need to re-sync
            icmd = None


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    main()
