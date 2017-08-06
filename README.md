
# About

m_net was a cross platform network library, provide a simple and
efficient interface for covenient use.

Support MacOS/Linux/Windows, using kqueue/epoll/select underlying.




# Features

- simple API with TCP/UDP support
- nonblocking & event driven interface
- using kqueue/epoll/select in MacOS/Linux/Windows




# Usage & Example

Feel free to drop plat_net.[ch] to your project, the example and test
case can be found in relative dir.

A more complex demo, the [m_tunnel](https://github.com/lalawue/m_tunnel)
project also using this library to provide a secure socks5 interface tcp
connection between local <-> remote side.




# Thanks

Thanks NTP source code from https://github.com/edma2/ntp, author: Eugene Ma (edma2)
