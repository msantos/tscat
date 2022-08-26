# SYNOPSIS

tscat *option* [*label*]

# DESCRIPTION

tscat: timestamp stdin to stdout/stderr

tscat timestamps standard input and writes the output to standard output,
standard error or both.

# EXAMPLES

    $ echo test | tscat
    2020-10-11T07:09:13-0400 test

    $ echo test | tscat foo
    2020-10-11T07:09:15-0400 foo test

    # duplicate output to stdout/stderr
    $ echo test | tscat -o 3 foo
    2020-10-11T07:09:15-0400 foo 2020-10-11T07:09:15-0400 foo test
    test

    $ echo test | tscat -o 3 foo > /dev/null
    2020-10-11T07:09:15-0400 foo test

    $ echo test | tscat -o 3 foo 2> /dev/null
    2020-10-11T07:09:15-0400 foo test

# Build

    make

    # selecting process restrictions
    RESTRICT_PROCESS=seccomp make

    #### using musl
    RESTRICT_PROCESS=rlimit ./musl-make

    ## linux seccomp sandbox: requires kernel headers

    # clone the kernel headers somewhere
    cd /path/to/dir
    git clone https://github.com/sabotage-linux/kernel-headers.git

    # then compile
    MUSL_INCLUDE=/path/to/dir ./musl-make clean all

# OPTIONS

-o, --output *1|2|3*
: stdout=1, stderr=2, both=3 (default: 1)

-f, --format *fmt*
: timestamp format (see strftime(3)) (default: `%F%T%z`)

-W, --write-error *exit|drop|block*
: behaviour if write buffer is full (default: block)

-h, --help
: usage summary

# ALTERNATIVES

~~~
#!/bin/sh

LABEL="${1-""}"
exec awk -v service="$LABEL" '{
  t = strftime("%FT%T%z")
  printf("%s %s %s\n", t, service, $0) > "/dev/stderr"
  printf("%s %s %s\n", t, service, $0)
  fflush()
}'
~~~
