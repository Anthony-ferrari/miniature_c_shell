# mini_c_shell
Mini C Shell is a miniature version of a c shell that allows foreground and background processes to be created for command line arguments such as exit, cd, status. Uses exec functions for other command line arguments. Makes use of child process spawning to handle command line arguments.  Also implements custom handlers for SIGINT and SIGTSTP. 


Flix is an app that allows users to browse movies from the [The Movie Database API](http://docs.themoviedb.apiary.io/#).

#### Features 
- [x] Allows foreground processes to run. The parent shell does not return command line access and control to user until child process terminates. 
- [x] Allows for background processes to run with & character. THe parent shell does return command line access and control to user after forking off the child process. 
- [x] Has built in functions for cd, status, and exit. All other commands are executed using the exec family functions. 
- [x] Implemented use of SIGINT for foreground process termination. 
- [x] Implemented use of SIGTSTP to disable the use of background processes. If user send a command with & after sending a SIGTSTP signal once, then it will be ignored. Another SIGTSTP signal being sent will counteract the last one sent.  

### Example

- $ mini_c_shell
- : ls
- junk   mini_c_shell    main.c
- : ls > junk
- : status
- exit value 0
- : cat junk
- junk
- mini_c_shell
- main.c
- : wc < junk > junk2
- : wc < junk
-       3       3      23
- : test -f badfile
- : status
- exit value 1
- : wc < badfile
- cannot open badfile for input
- : status
- exit value 1
- : badfile
- badfile: no such file or directory
: sleep 5
- ^Cterminated by signal 2
- : status &
- terminated by signal 2
- : sleep 15 &
- background pid is 4923
- : ps
-  PID TTY          TIME CMD
- 4923 pts/0    00:00:00 sleep
- 4564 pts/0    00:00:03 bash
- 4867 pts/0    00:01:32 mini_c_shell
- 4927 pts/0    00:00:00 ps
- :
- : # that was a blank command line, this is a comment line
- :
- background pid 4923 is done: exit value 0
- : # the background sleep finally finished
- : sleep 30 &
- background pid is 4941
- : kill -15 4941
- background pid 4941 is done: terminated by signal 15 
- : exit
- $

### Notes
I had difficulties terminating the child processes spawned by the parent background processes, but was able to terminate them by having saving the children into a list and having a preset condition to check the list to see if the child process was done so that I could terminate them. Also, had a hard time with multiple redirections in a command line but was able to implement it by taking one redirection at a time and having a boolean keeping track of it. 
