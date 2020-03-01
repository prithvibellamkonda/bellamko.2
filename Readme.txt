OSS
oss tests the primality of numbers where each number is tested by different child
process.

Prerequisites
    Tools need to build oss
        make
        gcc

Building:
    type the following commands to build the project
        cd path_to_oss_source_directory
        make

Running:
    usage: ./oss [-n number] [-s number] [-b number] [-i number] [-o path]

    -n      number of child process to launch. (default = 4)
    -s      max number of child process to run concurrently. (default = 2)
    -b      first number to check primality of. (min, default = 2)
    -i      increment value to get next number. (default = 1)
    -o      path to file, where output will be stored. (default = stdout)









