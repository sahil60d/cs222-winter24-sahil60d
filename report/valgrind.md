## Debugger and Valgrind Report

### 1. Basic information
 - Team #: 22
 - Github Repo Link: https://github.com/sahil60d/cs222-winter24-sahil60d.git
 - Student 1 UCI NetID: sjdesai
 - Student 1 Name: Sahil Desai
 - Student 2 UCI NetID (if applicable):
 - Student 2 Name (if applicable):


### 2. Using a Debugger
- Describe how you use a debugger (gdb, or lldb, or CLion debugger) to debug your code and show screenshots. 
For example, using breakpoints, step in/step out/step over, evaluate expressions, etc. 
  - I used the debugger all throughout this quarter to step through by code and trace errors, so I could correct them. I was esspecially helpful when handling segfaults because I would use breakpoints to pause the execution and be able to see exaclty what values are being used and where in memory I was accessing. Below is an example of how I'd use it to make sure the correct data is placed in the correct location in memory.
 
  - <img width="1792" alt="Screenshot 2024-03-19 at 6 40 00 PM" src="https://github.com/sahil60d/cs222-winter24-sahil60d/assets/40578247/e82fa86f-caff-4ca3-b7c8-a68ad4dbad72">
<img width="1792" alt="Screenshot 2024-03-19 at 6 40 13 PM" src="https://github.com/sahil60d/cs222-winter24-sahil60d/assets/40578247/11ffbc05-475c-47f7-81c1-38c7353f9009">



### 3. Using Valgrind
- Describe how you use Valgrind to detect memory leaks and other problems in your code and show screenshot of the Valgrind report.
  - I used valgrind to see where memory was leaking and to check if there were and Invalid reads. It is helpful see how much memory is still allocated after executing because then i was able to know where I had to free memory. The following code is an example of I valgrind outputs after I made sure that there were no memory leaks.
  - <img width="805" alt="Screenshot 2024-03-19 at 6 50 53 PM" src="https://github.com/sahil60d/cs222-winter24-sahil60d/assets/40578247/c554f06e-6bcc-4042-b748-584ee0158be9">
