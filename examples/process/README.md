
# About

two key functions

- mnet_multi_accept_balancer(), used for determine which worker can accept this new comming connection
- mnet_multi_reset_event(), used for create new event queue

Server as monitor `fork` 2 worker process then `waitpid`, the first worker will `exit` after `accept` more than 512 connections, then monitor will restart worker in 0.5s.

The demo only tesing under Mac/Linux.