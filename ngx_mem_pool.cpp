
#include"ngx_mem_pool.h"
#include <cmath>
#include <cstddef>
#include <iostream>
using std::endl;
using std::cout;
#include <memory_resource>
void *ngx_mem_pool:: ngx_create_pool(size_t size)
{
    ngx_pool_s *p;
    p=(ngx_pool_s*)malloc(size);
    if(p==nullptr)
    {
        return nullptr;
    }
    p->d.last=(u_char*)p+sizeof(ngx_pool_s);
    p->d.end=(u_char*)p+size;
    p->d.next=nullptr;
    p->d.failed=0;

    size=size-sizeof(ngx_pool_s);

    p->max=(size<NGX_MAX_ALLOC_FROM_POOL)? size:NGX_MAX_ALLOC_FROM_POOL;
    p->current=p;
    p->large=nullptr;
    p->cleanup=nullptr;
    pool_=p;
    return p;
}
//考虑字节对齐，从内存吃申请size大小内存

void *ngx_mem_pool::ngx_palloc(size_t size)
{
    if(size<=pool_->max)
    {
        return ngx_palloc_small(size,1);
    }
    return ngx_palloc_large(size);
}
//不考虑字节对齐
void*ngx_mem_pool::ngx_pnalloc(size_t size)
{

    if(size<=pool_->max)
    {
        return ngx_palloc_small(size,0);
    }
    return ngx_palloc_large(size);
}
//调用ngx_pallcoc实现内存分配，会初始化为0
void*ngx_mem_pool::ngx_pcalloc(size_t size)
{
    void *p;
    p=ngx_palloc(size);
    if(p)
    {
        ngx_memzero(p,size);
    }
    return p;
}
void*ngx_mem_pool::ngx_palloc_small(size_t size,ngx_uint_t align)
{
    u_char *m;
    ngx_pool_s *p;
    p=pool_->current;
    /*在循环内部，首先获取当前内存块的last指针，
     * 也就是可用内存的起始地址。如果需要对齐，
     * 就对m进行对齐调整。然后检查当前块剩余的空间是否足够分配请求的大小。
     * 如果足够，就更新last指针并返回分配的内存地址。
     * 如果不够，就移动到下一个内存块继续检查。
     * 如果所有块都不够，就调用ngx_palloc_block分配新的内存块。*/
    do
    { 

        m=p->d.last;
        if(align)
        {
            m=ngx_align_ptr(m,NGX_ALIGNMENT);
        }
        if((size_t)(p->d.end-m)>=size)
        {
            p->d.last=m+size;
            return m;
        }
        p=p->d.next;
    }
    while(p);
    return ngx_palloc_block(size);
}
//ngx_palloc_block是在现有内存块不足时，分配新块并链接到链表中。
//分配新的小块内存池
void*ngx_mem_pool::ngx_palloc_block(size_t size)
{
    u_char *m;
    size_t psize;
    ngx_pool_s *p,*newpool;
    //新块的大小是psize，即原始内存池的大小
    psize=(size_t)(pool_->d.end-(u_char*)pool_);
    m=(u_char*)malloc(psize);
    if(m==nullptr)
    {
        return nullptr;
    }

    newpool=(ngx_pool_s*)m;
    newpool->d.end=m+psize;
    newpool->d.next=nullptr;
    newpool->d.failed=0;
    /*m向后移动了ngx_pool_data_t结构体的大小，跳过了管理结构体的部分，
      指向了该内存块中可用于实际分配的内存区域的起始位置。*/

    m+=sizeof(ngx_pool_data_t);
    m=ngx_align_ptr(m,NGX_ALIGNMENT);
    newpool->d.last=m+size;
    for (p = pool_->current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            pool_->current = p->d.next;
        }
    }

    p->d.next = newpool;

    return m;
}



void*ngx_mem_pool::ngx_palloc_large(size_t size)
{
    void              *p;
    ngx_uint_t         n;
    ngx_pool_large_s  *large;

    p = malloc(size);
    if (p == nullptr) {
        return nullptr;
    }

    n = 0;

    for (large = pool_->large; large; large = large->next) {
        if (large->alloc == nullptr) {
            large->alloc = p;
            return p;
        }

        if (n++ > 3) {
            break;
        }
    }

    large =(ngx_pool_large_s*) ngx_palloc_small(sizeof(ngx_pool_large_s), 1);
    if (large == nullptr) {
        free(p);
        return nullptr;
    }

    large->alloc = p;
    large->next = pool_->large;
    pool_->large = large;

    return p;
}
//释放大块内存
void ngx_mem_pool::ngx_pfree(void *p)
{
    ngx_pool_large_s  *l;

    for (l = pool_->large; l; l = l->next) {
        if (p == l->alloc) {
            free(l->alloc);
            l->alloc = nullptr;

            return;
        }
    }

}

void ngx_mem_pool:: ngx_reset_pool()
{
    ngx_pool_s        *p;
    ngx_pool_large_s  *l;

    for (l = pool_->large; l; l = l->next) {
        if (l->alloc) {
            free(l->alloc);
        }
    }

    for (p = pool_; p; p = p->d.next) {
        p->d.last = (u_char *) p + sizeof(ngx_pool_s);
        p->d.failed = 0;
    }

    pool_->current = pool_;
    pool_->large = nullptr;
}

void ngx_mem_pool::ngx_destroy_pool()
{
    ngx_pool_s          *p, *n;
    ngx_pool_large_s    *l;
    ngx_pool_cleanup_s  *c;

    for (c = pool_->cleanup; c; c = c->next) {
        if (c->handler) {
            c->handler(c->data);
        }
    }
}

ngx_pool_cleanup_s* ngx_mem_pool::ngx_pool_cleanup_add(size_t size)
{
    ngx_pool_cleanup_s  *c;

    c =(ngx_pool_cleanup_s*) ngx_palloc(sizeof(ngx_pool_cleanup_s));
    if (c == nullptr) {
        return nullptr;
    }

    if (size) {
        c->data = ngx_palloc( size);
        if (c->data == nullptr) {
            return nullptr;
        }

    } else {
        c->data = nullptr;
    }

    c->handler = nullptr;
    c->next = pool_->cleanup;

    pool_->cleanup = c;


    return c;
}
//重置函数

