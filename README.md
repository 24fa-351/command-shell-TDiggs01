Create a command-line shell program like bash or Powershell. Have a prompt, example below.

Reads from input, writes to output. until "quit" or "exit".

Implement "cd" and "pwd". (directly). cd must take a full or relative path.

Implement the setting, deleting (unsetting) and getting of xsh environment vars:

xsh# set FOO banana
xsh# echo My favorite fruit is a $FOO.
My favorite fruit is a banana.
xsh# unset FOO
xsh# echo I'd love to eat a $FOO colored slug.
I'd love to eat a colored slug.
(this means that you should scan the command for a $<something> and replace it if found)

Read the PATH environment variable (from when the shell was started, not when/if it was set in xsh), and look for any command typed that is not directly implemented. Execute it when found; complain if not found.

Implement "|" to separate commands and pipe output from one to the input of another.

Implement "<" to pipe the contents of a file to the input of the command.

Implement ">" to pipe the command output to a file.

Implement "&" to run the command in the background. Otherwise, wait for the output.

(these three options can only be at the end of a command, but you can have 0-3 of them)

(Simplification: do not worry about quoted strings. No need to handle them properly.)

(don't worry about stderr, or piping/redirecting it)
