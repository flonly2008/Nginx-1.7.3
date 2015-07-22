
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <nginx.h>


ngx_int_t   ngx_ncpu;
ngx_int_t   ngx_max_sockets;
ngx_uint_t  ngx_inherited_nonblocking;
ngx_uint_t  ngx_tcp_nodelay_and_tcp_nopush;


struct rlimit  rlmt;


ngx_os_io_t ngx_os_io = {
    ngx_unix_recv,
    ngx_readv_chain,
    ngx_udp_unix_recv,
    ngx_unix_send,
    ngx_writev_chain,
    0
};

/**
 * 操作系统相关初始化
 *
 */
ngx_int_t
ngx_os_init(ngx_log_t *log)
{
    ngx_uint_t  n;
	//1.特定系统初始化
	/*auto_conf_config.h 会包含:
	 *NGX_FREEBSD,NGX_LINUX,NGX_SOLARIS,NGX_DARWIN,NGX_WIN32
	 * nginx.conf 文件会根据上面的定义引入:
	 * 		ngx_freebsd_config.h
	 * 		ngx_linux_config.h
	 * 		ngx_solaris_config.h
	 * 		ngx_darwin_config.h
	 * 		ngx_win32_config.h
	 * 		ngx_posix_config.h (未定义,则默认用posix)
	 * 		
	 * 		
	 * 		NGX_HAVE_OS_SPECIFIC_INIT 在darwin , freebsd, linux, solaris 里面定义为1
	 * 		作不同操作系统相关初始化
	 *   	ngx_os_specific_init 函数会定义在相应操作系统的.c文件中,比如ngx_os_darwin_init.c
	 */
#if (NGX_HAVE_OS_SPECIFIC_INIT)
    if (ngx_os_specific_init(log) != NGX_OK) {
        return NGX_ERROR;
    }
#endif

	// 2.为set proc title 准备空间
	//对linux 和 solaris, 将environ 拷贝到新内存, 为argv留出足够空间进行修改.
    ngx_init_setproctitle(log);

	
	//3.获取系统叶大小, cacheline大小
    ngx_pagesize = getpagesize();
	
	//cacheline size 来自autoconf
    ngx_cacheline_size = NGX_CPU_CACHE_LINE;

	//4.计算页大小是2的多少次方,  2^ngx_pagesize_shift = ngx_cacheline_size
    for (n = ngx_pagesize; n >>= 1; ngx_pagesize_shift++) { /* void */ }

	//5.矫正系统系统的cpu 个数 _SC_NPROCESSORS_ONLN 获取的是真实可用的cpu 核数,
	//参考http://www.2cto.com/kf/201210/164480.html
#if (NGX_HAVE_SC_NPROCESSORS_ONLN)
    if (ngx_ncpu == 0) {
        ngx_ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    }
#endif

    if (ngx_ncpu < 1) {
        ngx_ncpu = 1;
    }

	//5.根据cpu 类型矫正ngx_cacheline_size
    ngx_cpuinfo();

	//6.获取rlimit
	//参考:http://www.cnblogs.com/niocai/archive/2012/04/01/2428128.html
    if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        ngx_log_error(NGX_LOG_ALERT, log, errno,
                      "getrlimit(RLIMIT_NOFILE) failed)");
        return NGX_ERROR;
    }

    ngx_max_sockets = (ngx_int_t) rlmt.rlim_cur;

	//7.READMORE
#if (NGX_HAVE_INHERITED_NONBLOCK || NGX_HAVE_ACCEPT4)
    ngx_inherited_nonblocking = 1;
#else
    ngx_inherited_nonblocking = 0;
#endif

    srandom(ngx_time());

    return NGX_OK;
}


void
ngx_os_status(ngx_log_t *log)
{
    ngx_log_error(NGX_LOG_NOTICE, log, 0, NGINX_VER_BUILD);

#ifdef NGX_COMPILER
    ngx_log_error(NGX_LOG_NOTICE, log, 0, "built by " NGX_COMPILER);
#endif

#if (NGX_HAVE_OS_SPECIFIC_INIT)
    ngx_os_specific_status(log);
#endif

    ngx_log_error(NGX_LOG_NOTICE, log, 0,
                  "getrlimit(RLIMIT_NOFILE): %r:%r",
                  rlmt.rlim_cur, rlmt.rlim_max);
}


#if 0

ngx_int_t
ngx_posix_post_conf_init(ngx_log_t *log)
{
    ngx_fd_t  pp[2];

    if (pipe(pp) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "pipe() failed");
        return NGX_ERROR;
    }

    if (dup2(pp[1], STDERR_FILENO) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, errno, "dup2(STDERR) failed");
        return NGX_ERROR;
    }

    if (pp[1] > STDERR_FILENO) {
        if (close(pp[1]) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, errno, "close() failed");
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}

#endif
