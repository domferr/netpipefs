# NetpipeFS

## About
NetpipeFS let you mount a filesystem that allows processes to have a one-way communication over the network such that remote processes can communicate by writing and reading on their local files without managing a socket connection. Both processes just need to mount the filesystem and read or write from/to it to communicate. The file system will take care for them about network communication.

## How to use

Running netpipefs is very simple:

    netpipefs --port=[local_port] --host=[hostip] --hostport=[hostport] mountpoint

It is recommended to run NetpipeFS as regular user (not as root). For this to work the mountpoint must be owned by the user. If the port or hostport numbers are omitted NetpipeFS will use the default values.

Also many netpipefs options can be specified (see the [Options](/#options) section), including the connection timeout (``--timeout=MILLISECONDS``)

To unmount the filesystem:

    fusermount -u mountpoint

On BSD and macOS, to unmount the filesystem:

    umount mountpoint


## Build

First, download NetpipeFS from this repo. On Linux and BSD, you will also need to install [libfuse](http://github.com/libfuse/libfuse) 2.9.0 or newer. On macOS, you need [OSXFUSE](https://osxfuse.github.io/) instead. To build netpipefs, download the repo and run the following command in the main directory:

    $ make all
    
To run the test suite first build the tests by running ``make test`` and finally run ``make run_test`` to run the test suite.

## Options

| Option | Description |
| ---- | ---- |
| `-h, --help` | Print help and exit |
| `-d, --debug` | Print debugging information |
| `--port=PORT` | Port used for network communication |
| `--hostip=IP` | Host IP address |
| `--hostport=PORT` | Port used by host |
| `--timeout=MILLISECONDS` | Connection timeout. Expressed in milliseconds |
| `-f` | Do not daemonize, stay in foreground |
| `-s` | Single threaded operation |
| `-o reconnect` | Automatically reconnect to the server if the connection is interrupted. While NetpipeFS is not connected, attempts to do operations on files will return EAGAIN |
| `-o delay_connect` | Don't immediately connect to server, wait until mountpoint is first accessed |

NetpipeFS also accepts several options common to all FUSE file systems. See the [FUSE official repository](http://github.com/libfuse/libfuse) for further information.
