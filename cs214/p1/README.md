Jacob Park jdp231
Chris Shen cs1448

Testing Strategy: To test the output of our program, we created many different test cases that could be entered as a file, as stdin, and as entries in a directory. The contents of these test cases varied between standard (correct) input files and words that were longer than the maximum column length. For part 1, we wrapped the text to many different lengths to include all edge cases and tested only using edge cases to see how the program would preform. We tested the scenario of wrapping 1 word in the middle of the length, wrapping if there were less characters than the width of the wrapped page, and if the wrapped page ends on a space or in the middle of the word. For part two, most of the testing had to do with figuring out how to manage files via the POSIX functions. The logic for wrapping text was the same as part 1.

