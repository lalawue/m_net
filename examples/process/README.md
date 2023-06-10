
# About

Server as monitor `fork` 2 worker process then `waitpid`, the first worker will `exit` after `accept` more than 512 connections, then monitor will restart worker in 0.5s.

The demo only tesing under Mac/Linux.