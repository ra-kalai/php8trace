#!/bin/bash

bash --init-file <(cat <(cat <<"ø"
set -m # job control
_sendsigusr1tobgproc() {
  echo "send SIGUSR1 to $CMDPID"
  kill -SIGUSR1 $CMDPID
}
_sendsigusr2tobgproc() {
  echo "send SIGUSR2 to $CMDPID"
  kill -SIGUSR2 $CMDPID
}

_killbgproc() {
  echo 'kill proc'
  kill -SIGINT $CMDPID
}

_suspendbgproc() {
  echo 'suspend proc'
  kill -SIGTSTP $CMDPID
}

trap - TERM
trap - INT
trap _killbgproc INT
trap _suspendbgproc TSTP

set -o emacs
bind -x '"\C-y":"_sendsigusr1tobgproc"'
bind -x '"\C-u":"_sendsigusr2tobgproc"'
bind -x '"\C-c":"_killbgproc"'
bind -x '"\C-z":"_killbgproc"'
ø
) \
<(cat <<ø
$(/usr/local/bin/php-config --php-binary) -d extension=$(php-config --extension-dir)/observer.so -d observer.instrument=1 $@ &
ø
) \
<(cat <<'ø'
export CMDPID=$!
cat <<woot
starting proc: $CMDPID
control + t -> send siginfo to toggle verbosity
woot
ø
)
)
