# Checksummer
Checksummer is recursive file checksumming program. Its multi-threaded design allows for
fast checksumming of large amounts of files stored on solid-state media or RAID arrays.

## Usage
The program accepts options followed by a list of input paths as command line arguments.
Input paths can either point at files or directories. In the latter case, given directories
are scanned recursively and checksums of all files encountered are computed. Results are
printed on screen and optionally saved in a MySQL database.

The following options are recognized:
| Option        | Description                                                                                                                                                                        | Default value                                         |
|---------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------|
| `-c THREADS`  | Number of crawler threads to use.<br/><br/>Crawler threads are responsible for traversing the file system.                                                                         | 1                                                     |
| `-w THREADS`  | Number of worker threads to use.<br/><br/>Worker threads read files and calculate their checksums.                                                                                 | \<number of available processors\>                    |
| `-s THREADS`  | Number of collector threads to use.<br/><br/>Collector threads collect the results from worker threads, print them on screen and store them in the database.                       | 1                                                     |
| `-d DIGEST`   | Message digest algorithm to use. See `openssl list -digest-algorithms` for a list of supported ones.                                                                               | SHA256                                                |
| `-h HOSTNAME` | MySQL server hostname or UNIX domain socket path prefixed with `unix:`.                                                                                                            | \<empty; MySQL is not used\>                          |
| `-P PORT`     | MySQL server port.                                                                                                                                                                 | 3389                                                  |
| `-D DATABASE` | MySQL database to store results in.                                                                                                                                                | checksummer                                           |
| `-q QUERY`    | MySQL query to use.<br/><br/>The parameters bound to the query are `(checksum BINARY(n), path TEXT)`, where `n` is dependent on the message digest used (256 ÷ 8 = 32 for SHA256). | `INSERT INTO checksums (checksum, path) VALUES (?,?)` |
| `-u USER`     | MySQL user name.                                                                                                                                                                   | checksummer                                           |
| `-p`          | Ask for MySQL user password.                                                                                                                                                       |                                                       |

## Compiling
Checksummer depends on MariaDB Connector/C, OpenSSL, and pkg-config. The dependencies can be installed
with the following commands:
```
# Fedora
sudo dnf install @c-development mariadb-connector-c-devel openssl-devel
# CentOS/RHEL
sudo yum install gcc make mariadb-connector-c-devel openssl-devel
# Debian/Ubuntu
sudo apt install build-essential libcrypto-dev libmariadb-dev pkg-config
```
Afterwards, the program can be built running `make`.
