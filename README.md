c语言实现consumer-agent，而producer-agent依旧是java。具体文档后面再更新，先废话几句：


啊~~~~！！！！当时可是很自信地摒弃libevent,基于epoll精心手撸了一个线程模型。。啊，脸真疼！手也很疼！嗯，不错，手撸了线程池、内存池、线程栈内存池、http服务器和etcd接口封装等等等等！！！！要不是很无脑地踩到那个大坑(http keep-alive)，，我就........
