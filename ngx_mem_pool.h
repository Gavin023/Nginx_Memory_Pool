#pragma once
//类型重定义
#include <cstddef>
#include <cstdint>
#include<cstdlib>
#include <sys/types.h>
#include<string.h>
using u_char =unsigned char;
using ngx_uint_t=unsigned int;

/*
   移植nginx内存池代码 oop实现
   */ 
typedef void (*ngx_pool_cleanup_pt)(void *data); // 清理回调函数的类型定义
                                                 // 清理操作的类型定义，包括一个清理回调函数，传给回调函数的数据和下一个清理操作的地址
struct ngx_pool_cleanup_s {
    ngx_pool_cleanup_pt handler; // 清理回调函数 函数指针
    void *data; // 传递给回调函数的指针
    ngx_pool_cleanup_s *next; // 指向下一个清理操作 大块内存存储在链表
};


//大块内存头部信息
struct ngx_pool_large_s
{
    ngx_pool_large_s *next; //大块内存在链表存储
    void *alloc;//保存分配出去大块内存起始地址
};

/*
   分配小块内存的内存池的头部数据信息
   */
//类型前置声明
struct ngx_pool_s;
struct ngx_pool_data_t 
{
    u_char *last;
    u_char *end;//小块可用内存末尾地址
    ngx_pool_s *next;// 
    ngx_uint_t failed;//小块内存分配失败情况
};


// nginx内存池的主结构体类型
struct ngx_pool_s {
    ngx_pool_data_t d; //存储当前小块内存使用情况
    size_t max; // 小块内存和大块内存分界线
    ngx_pool_s *current; // 小块内存池入口指针
    ngx_pool_large_s *large; // 大块内存分配入口指针
    ngx_pool_cleanup_s *cleanup;
};
//把数值d调整成临近a的倍数
#define ngx_align(d,a) (((d)+(a-1)) & ~(a-1))
//小内存分配考虑字节对齐的单位
#define NGX_ALIGNMENT sizeof(unsigned long)
//把指针p调整到临近a的倍数
#define ngx_align_ptr(p, alignment) \
    (u_char*)(((uintptr_t)(p) + (alignment - 1)) & ~(alignment - 1))
#define ngx_memzero(buf, n)       (void) memset(buf, 0, n)
const int ngx_pagesize=4096;
const int NGX_MAX_ALLOC_FROM_POOL=ngx_pagesize-1;
const int  NGX_DEFAULT_POOL_SIZE =16*1024;
//内存池按照16字节对齐
const int NGX_POOL_ALIGNMENT=16;
//ngx小块内存池最小size调整成ngx_pool_alignment的临近倍数
const int NGX_MIN_POOL_SIZE=
ngx_align((sizeof(ngx_pool_s)+2*sizeof(ngx_pool_large_s)),
          NGX_POOL_ALIGNMENT);

class ngx_mem_pool
{ 
public:
    ngx_mem_pool() {}
    ~ngx_mem_pool() {}

    void * ngx_create_pool(size_t size);
    //考虑字节对齐，从内存吃申请size大小内存
    void *ngx_palloc(size_t size);
    //不考虑字节对齐
    void *ngx_pcalloc(size_t size);
    //释放大块内存

    void *ngx_pnalloc(size_t size);  
    void ngx_pfree(void *p);
    //重置函数
    void ngx_reset_pool();

    void ngx_destroy_pool();
    ngx_pool_cleanup_s *ngx_pool_cleanup_add(size_t size);

private:
    ngx_pool_s *pool_;
    void *ngx_palloc_small(size_t size,ngx_uint_t align);
    void *ngx_palloc_large(size_t size);
    //分配新的小块内存池
    void *ngx_palloc_block(size_t size);
};

