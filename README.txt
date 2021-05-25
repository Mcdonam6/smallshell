################################################
Compiling and Running the smallsh program 
################################################

---Prerequisites:---
In order to compile and run the smallsh shell program, first ensure the system you are
using has the gcc compiler installed. If it does not, the gcc commpiler install 
instructions can be found at the following address: 

https://gcc.gnu.org/install/index.html

---Compiling smallsh.c into smallsc---
To compile the main.c file into an executable movies_by_year program, navigate to the directory containing the main.c
file using your system's command line interface, and run the following command:

gcc --std=gnu99 -o smallsh smallsh.c

---Running the compiled smallsh program---
After successfully compiling the program, run the program by entering the program name smallsh in the command line. 
No other arguments are necessary. The program will then begin and provide interactivity to the user with a ': ' prompt. 
Built in commands for the interactive shell include cd (change directory), status (present the status of the last foreground 
process to finish) and exit (end all running processes and exit the shell program). Other standard unix shell commands are
also compatible with this shell interface.   

--File run Command:
smallsh