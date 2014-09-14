#!/bin/bash

export RK_SERVERS=localhost:8087

testList()
{
  lines=`./bin/rk ls | wc -l`
  #assertEquals "List test failed." 0 $rc
  assertTrue "List test failed." "[ 1 -lt $lines ]"
}

# load shunit2
if [ -f /usr/share/shunit2/shunit2 ]; then
  . /usr/share/shunit2/shunit2
else
  echo "SKIPPING: shunit2 not found."
  exit 77
fi

exit
./bin/rk -s "${RIAK_HOST1}:$RIAK_PORT1" -s "${RIAK_HOST2}:$RIAK_PORT2" map -t js -f src/tests/data/test_search.json
./bin/rk cat -h code riak_put.c
./bin/rk add code api.c -f src/riak/api.c
./bin/rk add code configure < configure
./bin/rk add code foo.c -f foo.c
./bin/rk cat code api.c
./bin/rk cat code api.c|wc
./bin/rk cat code configure
./bin/rk cat code foo.c
./bin/rk cat code riak_put.c
./bin/rk cat test get_put_test_key
./bin/rk cat -x code '7269616b5f7075742e63'
./bin/rk cat -x code riak_put.c
./bin/rk cat code foo.c
./bin/rk ls code
./bin/rk ls
