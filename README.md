# fspipe

## About
Fspipe allows you to mount a filesystem that allows processes to have a one-way communication over the network such that one process writes to a file, and the other remote process gets the data by reading from its file. Both processes just need to mount the filesystem and read or write from/to it to communicate. The file system will take care of the network communication for them.
