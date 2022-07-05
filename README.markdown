php8trace
=============

About
-----
php8trace is a command line php8 script tracer, using the PHP8 observer API
It allow you to watch what a running php script is doing

b.php
```php
<?php
function takearrayandobjectreturnarray($arg, $arg2) {
	return [1,2,3];
}
function retstringtakearray($arg) {
	takearrayandobjectreturnarray($arg, (object)$arg);
	return 'aaa';
}
function call1($arg) {
	retstringtakearray(['test' => $arg+2]);
	return [1,2,3];
}

call1(2);
```

```php
php8trace -d observer.instrument_dump_execute_data_ptr=0 b.php

starting php process with pid [ 115549 ]
> call1((int)2,) [/usr/local/src/app/b.php:10]
  > retstringtakearray((array)[test => (int)4,]#1,) [/usr/local/src/app/b.php:6]
    > takearrayandobjectreturnarray((array)[test => (int)4,]#1,(stdClass)#1,) [/usr/local/src/app/b.php:3]
    < takearrayandobjectreturnarray((array)[test => (int)4,]#1,(stdClass)#1,) [/usr/local/src/app/b.php:3]: (array)[0 => (int)1,1 => (int)2,2 => (int)3,]#3 (0.000954)
  < retstringtakearray((array)[test => (int)4,]#1,) [/usr/local/src/app/b.php:7]: (string)"aaa" (0.026941)
< call1((int)2,) [/usr/local/src/app/b.php:11]: (array)[0 => (int)1,1 => (int)2,2 => (int)3,]#3 (0.047922)

```


To try
--------------------

make install
