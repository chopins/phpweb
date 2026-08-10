1. 给PHP添加一个SAPI模块，PHP版本是8.3以后的版本，以前的版本不考虑
2. 该模块在linux平台使用libwebkitgtk 6.0以上版本，gtk使用 libgtk4.0以上版本,
3. 在windows平台使用webview2 后的较新版本，必须是支持windows11,与windows10以后的版本，以前的系统不考虑
4. 利用前面第2条给出的相关webview库实现PHP执行结构输出到 webview中去显示的功能，
5. 现在给出可用的代码，
6. 不要提供另外的文字说明，只给出代码，可在代码关键处注释
7. 同时给出config.m4与config.w32 等configure与编译必须文件
8. SAPI模块名字叫 phpweb, 可执行文件名为 phpweb
9. 添加类似CLI模块的 -c、-n、 -d、-z 参数，支持相关功能
10. 添加类似CLI模块的 -m、-v，--ini 参数，信息输出到打开的窗口的 webview中
11. 添加类似CLI模块 -i 参数，信息输出使用 phpinfo()函数输出内容，信息输出到打开的窗口的 webview中
12. 添加类似CLI模块的 -h参数，帮助信息显示到控制台中，而不启动窗口
13. 添加类似CLI模块的 -f 、-F 、-r、-R 、-B、-E 参数，执行php代码后，将输出信息输出到 webview中
14. 所有php代码输出信息，错误报告信息全部输出到webview中
15. 添加类似CLI模块的args... 处理功能，传递给php代码并创建 $argc与$argv $_SERVER等变量
16. 在 webview 的上下文菜单中添加打开文件菜单，对于打开的php 文件将直接执行，并将输出显示到webview中
17. 在 webview 的上下文菜单中添加打开刷新菜单，点击后将重新执行当前php文件
18. 在webview中，HTML页面指向的并本地URL路径，将使用php打开并执行php文件
19. webview打开的不是php文件则直接显示，html让webview渲染
20. 添加类似CLI的 -t 参数指定根目录，对于html中的相对路径则以该根目录为相对路径
21. 如果在打开html URL路径时，执行php代码需要相关协议支持，则注册 php:// 这个协议进行相关处理
22. 如果要使用 base_url则，将其设置为 php://exec/加上根目录，如何没有设置根目录，则使用当前工作目录
23. 在 webview 的上下文菜单中添加退出菜单
24. 在webview的上下文菜单中添加 -m、-v，--ini 参数的功能菜单
25. 在webview的上下文菜单中添加帮助菜单，输出内容与-h参数一致，但是内容输出到webview中
26. 内容改变后是变更webview显示，而不是重新启动 SAPI
27. 执行PHP configure 后，要能在 phpweb 这个SAPI 目录生成相关makefile等编译所需要的文件
28. webview 的上下文菜单使用英文