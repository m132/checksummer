# Checksummer
Checksummer is recursive file checksumming program. Its multi-threaded design allows for
fast checksumming of large amounts of files stored on solid-state media or RAID arrays.

## Usage
The program accepts options followed by a list of input paths as command line arguments.
Input paths can either point at files or directories. In the latter case, given directories
are scanned recursively and checksums of all files encountered are computed. Results are
printed on screen and optionally saved in a MySQL database.

The following options are recognized:
| Option        | Description                                                                               | Default value                                         |
|---------------|-------------------------------------------------------------------------------------------|-------------------------------------------------------|
| `-c THREADS`  | number of crawler threads to use                                                          | 1                                                     |
| `-w THREADS`  | number of worker threads to use                                                           | \<number of available processors\>                    |
| `-s THREADS`  | number of collector threads to use                                                        | 1                                                     |
| `-d DIGEST`   | message digest to use; see `openssl list -digest-algorithms` for a list of supported ones | SHA256                                                |
| `-h HOSTNAME` | MySQL server hostname                                                                     | localhost                                             |
| `-P PORT`     | MySQL server port                                                                         | 3389                                                  |
| `-D DATABASE` | MySQL database to store results in                                                        | checksummer                                           |
| `-q QUERY`    | MySQL query to use                                                                        | `INSERT INTO checksums (checksum, path) VALUES (?,?)` |
| `-u USER`     | MySQL user name                                                                           | checksummer                                           |
| `-p`          | ask for MySQL user password                                                               |                                                       |

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
