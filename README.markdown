php8trace
=============

About
-------
php8trace is a command line php8 script tracer, using the PHP8 observer API
It allow you to watch what a running php script is doing

#### Example

b.php
```php
function takeanarrayandobjectandreturnarray($arg, $arg2) {
	return [1, $arg, $arg2];
}
function takearrayandreturnstring($arg) {
	takeanarrayandobjectandreturnarray($arg, (object)$arg);
	return 'foo';
}

function initial_call($arg1, $arg2) {
	return takearrayandreturnstring(['bar' => $arg2+2]);
}

initial_call('initial', 1);
```

```bash
php8trace b.php
```

```
starting php process with pid [ 425915 ]
> initial_call((string)"initial",(int)1,) [/home/ralauge/cohiba/maduro/b.php:11] 0x7F1CA7812080
  > takearrayandreturnstring((array)[bar => (int)3,]#1,) [/home/ralauge/cohiba/maduro/b.php:6] 0x7F1CA7812120
    > takeanarrayandobjectandreturnarray((array)[bar => (int)3,]#1,(stdClass)#1,) [/home/ralauge/cohiba/maduro/b.php:3] 0x7F1CA78121A0
    < takeanarrayandobjectandreturnarray((array)[bar => (int)3,]#1,(stdClass)#1,) [/home/ralauge/cohiba/maduro/b.php:3]: (array)[0 => (int)1,1 => (array)[bar => (int)3,]#1,2 => (stdClass)#1,]#3 0x7F1CA78121A0 (0.020981)
  < takearrayandreturnstring((array)[bar => (int)3,]#1,) [/home/ralauge/cohiba/maduro/b.php:7]: (string)"foo" 0x7F1CA7812120 (0.112057)
< initial_call((string)"initial",(int)1,) [/home/ralauge/cohiba/maduro/b.php:11]: (string)"foo" 0x7F1CA7812080 (0.202894)

```


To try
--------------------

#### Install
```bash
make install
php8trace script.php
```

#### Added php ini settings
  | default ini when using the php8trace cmd wrapper | description                                                  |
  |:-------------------------------------------------|:-------------------------------------------------------------|
  | -d `observer.instrument=1`                       | enable module                                               |
  | -d `observer.instrument_dump_timing=1`           | dump the time taken by function/call                         |
  | -d `observer.instrument_dump_execute_data_ptr=1` | dump a ptr to the current struct `execute_data`.<br />This ptr doesn't change between the start, and the return of a function; so it allow us to quickly jump betwen the two on a log file |

#### Interesting keyboard shortcut of php8trace
##### standard 
  - `Control + s`: Default XOFF behavior
  - `Control + q`: Default XON behavor
  - `Control + c`: Forward SIGINT to the underlying php process
  - `Control + z`: Forward SIGTSTP to the underlying php process

<!---->
##### added one

  - `Control + w`: Forward SIGCONT to the underlying  php process
  - `Control + y`: toggle the silent mode - trace are no longer output on `stderr`, the script run quicker
  - `Control + u`: toggle the argument / return value pretty dump, script run quicker


Others
--------

`php8trace.sh` is a pure bash wrapper, but its kinda hard to use
  - The stdout / stderr - shell pipe redirect does not work properly because the php process is started in background.
    To create a log file the easiest way would seem to use the `script` command
  - When the PHP process die,  the user is left inside a subshell
