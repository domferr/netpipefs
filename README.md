# NetpipeFS

## About
NetpipeFS let you mount a filesystem that allows processes to have a one-way communication over the network such that remote processes can communicate by writing and reading on their local files without managing a socket connection. Both processes just need to mount the filesystem and read or write from/to it to communicate. The file system will take care for them about network communication.

## How to use

Running netpipefs is very simple:

    netpipefs --port=[local_port] --host=[hostip] --hostport=[hostport] mountpoint

It is recommended to run NetpipeFS as regular user (not as root). For this to work the mountpoint must be owned by the user. If the port or hostport numbers are omitted NetpipeFS will use the default values.

Also many netpipefs options can be specified (see the [Options](/#options) section), including the connection timeout (``--timeout=MILLISECONDS``).

To unmount the filesystem:

    fusermount -u mountpoint

On BSD and macOS, to unmount the filesystem:

    umount mountpoint

NetpipeFS can also be run with valgrind to check for memory leaks:

    valgrind --sim-hints=fuse-compatible netpipefs --port=[local_port] --host=[hostip] --hostport=[hostport] mountpoint

To check if the filesystem is mounted:

    mount | grep netpipefs


## Build

First, download NetpipeFS from this repo. On Linux and BSD, you will also need to install [libfuse](http://github.com/libfuse/libfuse) 2.9.0 or newer. On macOS, you need [OSXFUSE](https://osxfuse.github.io/) instead. To build netpipefs, run the following command in the main directory:

    $ make all
    
To run the test suite first build the tests by running ``make test`` and finally run ``make run_test`` to run the test suite.

## Options

| Option | Description |
| ---- | ---- |
| `-h, --help` | Print help and exit |
| `-d, --debug` | Print debugging information |
| `-p PORT, --port=PORT` | Port used for network communication |
| `--hostip=IP` | Host IP address |
| `--hostport=PORT` | Port used by host |
| `--timeout=MILLISECONDS` | Connection timeout. Expressed in milliseconds |
| `--pipecapaciy=CAPACITY` | Maximum network pipe size |
| `-f` | Do not daemonize, stay in foreground |
| `-s` | Single threaded operation |

NetpipeFS also accepts several options common to all FUSE file systems. See the [FUSE official repository](http://github.com/libfuse/libfuse) for further information.

## Examples

To show what NetpipeFS can do and the usage of network pipes, there are several examples in the `examples` directory.
Usually the examples work with two mountpoints but they don't mount the filesystem, so run NetpipeFS before running the examples.
