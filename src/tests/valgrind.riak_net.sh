#!/bin/bash

VERR=tests/valgrind.riak_net.$$.err
TESTBIN=tests/riak_net

if type -f valgrind > /dev/null; then
  trap "rm -f $VERR" INT TERM EXIT
  valgrind --leak-check=full $TESTBIN 2> $VERR
  proc_err=$?
  if [[ $proc_err != 0 ]]; then
    exit $proc_err
  fi
  if grep -q 'ERROR SUMMARY: 0 errors' $VERR > /dev/null 2>&1; then
    echo valgrind PASS
    exit 0
  else
    echo valgrind FAIL
    echo Rerun with valgrind --leak-check=full $TESTBIN
    exit 1
  fi
else
  echo vlagrind missing
  exit 77
fi
