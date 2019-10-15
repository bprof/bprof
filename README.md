# bprof

[![](https://img.shields.io/pypi/v/bprof.svg)](https://pypi.python.org/pypi/bprof)
[![](https://img.shields.io/travis/joelfrederico/bprof.svg)](https://travis-ci.org/joelfrederico/bprof)
[![Documentation Status](https://readthedocs.org/projects/bprof/badge/?version=latest)](https://bprof.readthedocs.io/en/latest/?badge=latest)

A Better PROFiler


* Free software: MIT license
* Documentation: https://bprof.readthedocs.io.


## Introduction

The reason bprof exists is that the major Python profiling packages simply don't profile robustly. They use timestamps and ad-hoc methods for keeping track of how time passes. For example, one approach is to timestamp when a function starts and when it stops, and then count this as the function time. This includes time spent in the profiling hooks.

The approach taken here is to integrate all of the time between hooks and add it to the appropriate records. By registering for all hooks except for opcodes, the time spent out of the profiler is directly measured. The time is measured right after entering bprof, and right before exiting. This allows for as rigorous time measurement as possible. Then, stacks are used to track the current contexts and record detailed profiling information.

## Example

Code:

```python
import a
import b
b.c(yes="no")
```

Results:

```
    $ Name: f, 1.2142e-05
    $ 1.00074(6.21e-07/1.00074):     time.sleep(1)
    $ Name: <built-in function stop>, 0
    $ Name: <built-in function sleep>, 1.00074
```

## Future

There is a lot of future work. The display is rudimentary and not programmatically accessible. There are crashes in some scenarios on running invalid code, or upon exit. This is just a first pass.

* Function counts
* Save statistics

## Credits

This package was created with [Cookiecutter][1] and the [audreyr/cookiecutter-pypackage][2] project template.

[1]: https://github.com/audreyr/cookiecutter
[2]: https://github.com/audreyr/cookiecutter-pypackage
