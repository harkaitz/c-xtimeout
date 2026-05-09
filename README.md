# XTIMEOUT

Run a command with a time limit, a popup appears before killing the program.

    Usage: xtimeout [OPTS...] [-x POPUP_MESSAGE] DURATION COMMAND...
    
    Start COMMAND, [show a popup and] kill it if still running after DURATION.
    
      -V           : Dump configuration (environment variables).
      -k DURATION  : Also send a KILL if running after the initial signal.
      -s NUMBER    : Terminate with this signal, by default SIGTERM.
      -n MESSAGE   : Send a notification when timed out.
      -X DURATION  : Popup message timeout, by default one minute.
    
    Duration format: 1w2d3h30m, 3h50m, ...
    When the program times out the exit status is 124.

## Configuration.

By default the program uses `xmessage` and `notify-send`, but you can
set the following environment variables to customize which programs
to use.

The `$XTIMEOUT_POPUP` which by default is:

    xmessage -buttons Kill:0,Keep:2 ${__TIMEOUT:+ -timeout "${__TIMEOUT}" } -center "${__MESSAGE}" >/dev/null

The `$NOTIFY_SEND` which by default is:

    notify-send -a

The `$NOTIFY_SEND` environment variable should have a command that
receives 3 parameters, the program name, the title and the body, such
as:

    $NOTIFY_SEND xtimeout "${__TITLE}" "${__MESSAGE}"

## Style guide

This project follows the OpenBSD kernel source file style guide (KNF).

Read the guide [here](https://man.openbsd.org/style).

## Collaborating

Feel free to open bug reports and feature/pull requests.

More software like this here:

1. [https://harkadev.com/prj/](https://harkadev.com/prj/)
2. [https://devreal.org](https://devreal.org)

