package = 'mnet'
version = 'openssl-1'
source = {
   url = 'git+https://github.com/lalawue/m_net.git',
   --url = 'git+https://gitee.com/lalawue/m_net.git',
   tag = '0.4.20220807',
}
description = {
   summary = 'Cross platform network library support pull style API for Lua/LuaJIT',
   detailed = [[
      mnet is a cross platform network library, support pull style API,
      using epoll/kqueue/wepoll underlying, support Lua/LuaJIT.
   ]],
   homepage = 'https://github.com/lalawue/m_net',
   maintainer = 'lalawue <suchaaa@gmail.com>',
   license = 'MIT/X11'
}
dependencies = {
   "lua >= 5.1",
}
build = {
   type = "builtin",
   modules = {
      ["mnet"] = {
         sources = { "src/mnet_core.c", "extension/lua/mnet_lua.c", "extension/mdns/mdns.c", "extension/openssl/mnet_tls.c" },
         incdirs = { "src", "extension/openssl", "$(OPENSSL_INCDIR)" },
         libraries = { "ssl" }
      },
      ["mnet-chann"] = "extension/lua/mnet-chann.lua",
      ["ffi-mnet"] = "extension/luajit/ffi-mnet.lua",
      ["ffi-mdns"] = "extension/mdns/ffi-mdns.lua",
   }
}
