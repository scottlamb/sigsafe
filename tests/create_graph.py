#!/usr/bin/env python
"""create_graph - creates a bar graph comparing the read() microbenchmarks.

This will run the raw read(), sigsafe_read(), and select()+read()
microbenchmarks five times each. It will generate a gnuplot file to plot a
95% confidence interval for the mean user CPU time of each."""

import os
import resource

"""Computes the user CPU time required by a single run of the given test.
Returns the time, in seconds."""
def computeUserTime(test_name):
    user_time_before = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime
    retval = os.spawnl(os.P_WAIT, './' + test_name, test_name)
    if retval != 0:
        raise 'Test %s failed with return value %d' % (test_name, retval)
    user_time_after = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime
    return user_time_after - user_time_before

tests = [('bench_read_raw',   'Raw read()'),
         ('bench_read_safe',  'sigsafe_read()'),
         ('bench_read_select','select()+read()')]

raw_time = computeUserTime('bench_read_safe')
print "Raw time: %f" % (raw_time)
