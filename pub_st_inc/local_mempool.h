/*
 * local_mempool.h
 *
 *  Created on: 2014-9-11
 *      Author: liwei
 */

#ifndef LOCAL_MEMPOOL_H_
#define LOCAL_MEMPOOL_H_
#include "public.h"
#include "db_chain.h"
#include "spinlock.h"
#define block_size (64*1024)
#define cache_size  256
#define max_free_block 16;

#define mem_flag_lock 0x00000001
#define mem_flag_small 0x00000002
#define page_mask ((1<<12)-1)
struct lo_mempool;
typedef struct lo_mempool_block
{
	void *start;
	void *buf_start;
	struct lo_mempool *pool;
	chain_node node;
	unsigned short free_count;
	unsigned short all_count;
	unsigned short first_free_pos;
	unsigned short pos_clum[1];
}mempool_block;
typedef struct lo_mempool
{
	size_t len;
	unsigned long dt_size;
	unsigned int flags;
	unsigned int blocksize;
	unsigned int align_len;
	unsigned int buf_dt_count;
	unsigned int start_pos;
	chain_head free_block_list;
	chain_head using_block_list;
	chain_head used_block_list;
	unsigned int cache_top;
	void *cache[cache_size];
	arch_spinlock_t lock;
}mempool;
typedef struct _mp_4k_buf
{
	mempool_block * block;
	unsigned short free_count;
	unsigned short all_count;
	unsigned short first_free_pos;
	unsigned short pos_clum[1];
}mp_4k_buf;
static inline void init_mempool(mempool *pool,unsigned int flag,unsigned int dt_size,unsigned int align_len)
{
	c_init_chain(&pool->used_block_list);
	c_init_chain(&pool->free_block_list);
	c_init_chain(&pool->using_block_list);
	pool->cache_top=0;
	pool->flags=flag;
	pool->align_len=align_len;
	pool->blocksize=block_size;
	if(align_len>0)
		pool->dt_size=ALIGN(dt_size,align_len);
	else
		pool->dt_size=dt_size;

	pool->buf_dt_count=(4096-sizeof(mp_4k_buf))/(sizeof(unsigned short)+pool->dt_size);
	if(pool->align_len>0)
	{
		if(pool->buf_dt_count*pool->dt_size+ALIGN(pool->buf_dt_count*sizeof(unsigned short)+sizeof(mp_4k_buf),pool->align_len)>4096)
			pool->buf_dt_count--;
		pool->start_pos=ALIGN(pool->buf_dt_count*sizeof(unsigned short)+sizeof(mp_4k_buf),pool->align_len);
	}
	else
		pool->start_pos=pool->buf_dt_count*sizeof(unsigned short)+sizeof(mp_4k_buf);
	if(flag&mem_flag_lock)
		spin_lock_init(&pool->lock);
}
static inline int destory_mempool(mempool *pool)
{
	mp_4k_buf *buf;
	mempool_block *block;
	if(pool->cache_top>0)
	{
		int offset=0;
		void *cache_mem;
		do
		{
			buf=(mp_4k_buf*)((unsigned long)(cache_mem=pool->cache[pool->cache_top--])&(~page_mask));
			block=buf->block;
			offset=(cache_mem-((void*)buf+pool->start_pos))/pool->dt_size;
			buf->pos_clum[offset]=buf->first_free_pos;
			buf->first_free_pos=offset;
			buf->free_count++;
			if(buf->free_count==buf->all_count)
			{
				offset=((void*)buf-block->start)/4096;
				block->pos_clum[offset]=block->first_free_pos;
				block->first_free_pos=offset;
				block->free_count++;
				if(block->free_count==block->all_count)
				{
					c_delete_node(&pool->using_block_list,&block->node);
					c_insert_in_head(&pool->free_block_list,&block->node);
				}
			}
		}while(pool->cache_top>0);
	}
	while(NULL!=(block=get_last_dt(&pool->free_block_list,mempool_block,node)))
	{
		c_delete_node(&pool->free_block_list,&block->node);
		if((void*)block<block->buf_start||(void*)block>block->buf_start+pool->blocksize)
		{
			free(block->buf_start);
			free(block);
		}
		else
			free(block->buf_start);
	}
	if(!c_is_empty(&pool->using_block_list)||!c_is_empty(&pool->used_block_list))
		return -1;
	return 0;
}
static inline mempool_block* get_new_block(mempool *pool)
{
	void *addr,*saddr,*eaddr;
	mempool_block * block;
	unsigned int block_st_size=sizeof(mempool_block)+sizeof(unsigned short)*block_size/pool->dt_size;
	if(NULL==(addr=malloc(block_size)))
		return NULL;
	saddr=(void*)((unsigned long)addr&(~page_mask))+4096;
	if(saddr-addr>=block_st_size)
	{
		block=(mempool_block *)addr;
		eaddr=addr+block_size;
	}
	else if(((unsigned long)(addr+block_size)&(page_mask))>=block_st_size)
	{
		block=(void*)((unsigned long)(addr+block_size)&(~page_mask))+1;
		eaddr=block-1;
	}
	else
	{
		block=(mempool_block*)malloc(block_st_size);
		eaddr=addr+block_size;
	}
	block->start=saddr;
	block->buf_start=addr;
	block->pool=pool;
	block->free_count=block->all_count=(saddr-eaddr)/pool->dt_size;
	for(int i=0;i<block->free_count-1;i++)
	{
		block->pos_clum[i]=i+1;
	}
	block->pos_clum[block->free_count]=-1;
	block->first_free_pos=0;
	c_insert_in_head(&pool->free_block_list,&block->node);
	return block;
}
//创建一个新的4k模式的block，block中每个buf的大小是4k，buf的开头是mp_4k_buf结构体
static inline mempool_block* get_new_block_4k(mempool *pool)
{
	void *addr,*saddr,*eaddr;
	mempool_block * block;
	mp_4k_buf *buf;
	//获取block结构体自身的大小
	unsigned int block_st_size=sizeof(mempool_block)+sizeof(unsigned short)*block_size/4096;
	//获得一个buf中能存放的单元的数量
	if(NULL==(addr=malloc(block_size)))//创建block
		return NULL;
	//取得addr后第一个4k地址
	saddr=(void*)((unsigned long)addr&(~page_mask))+4096;
	if(saddr-addr>=block_st_size)//如果addr与saddr之间的空间可以存放block结构体，就将block放在开头
	{
		block=(mempool_block *)addr;
		eaddr=addr+block_size;
	}
	else if((((unsigned long)(addr+block_size))&(page_mask))>=block_st_size)//如果结尾到结尾前的第一个4k地址之间有足够的空间，就将block结构体放在末尾
	{
		block=(void*)(((unsigned long)(addr+block_size))&(~page_mask))+1;
		eaddr=block-1;
	}
	else //头和尾都没有足够的空间，就单独创建
	{
		block=(mempool_block*)malloc(block_st_size);
		eaddr=addr+block_size;
	}
	block->start=saddr;
	block->buf_start=addr;
	block->pool=pool;
	block->free_count=block->all_count=(eaddr-saddr)/4096;
	//初始化block的链表
	for(int i=0;i<block->all_count;i++)
	{
		block->pos_clum[i]=i+1;
		buf=((mp_4k_buf*)(block->start+4096*i));
		buf->free_count=buf->all_count=pool->buf_dt_count;
		buf->block=block;
		//初始化buf的链表
		for(int j=buf->all_count-1;j>=0;j--)
			buf->pos_clum[j]=j+1;
		buf->pos_clum[buf->all_count-1]=-1;
		buf->first_free_pos=0;
	}
	block->pos_clum[block->all_count-1]=-1;
	block->first_free_pos=0;
	c_insert_in_head(&pool->free_block_list,&block->node);
	return block;
}
static inline void *get_mem(mempool *pool)
{
	void *mem;
	if(pool->flags&mem_flag_lock)//如果带锁，则加锁
		arch_spin_lock(&pool->lock);
	if(pool->cache_top>0)//如果cache中有剩余的，直接从cache中取
	{
		mem=pool->cache[pool->cache_top--];
		if(pool->flags&mem_flag_lock)
				arch_spin_unlock(&pool->lock);
		return mem;
	}
	//cache已经为空，填充cache
	int balance=cache_size/2;
	while(pool->cache_top<balance)
	{
		mempool_block *block;
		mp_4k_buf * buf;
		//优先从未使用完的block上取单元
		if(!c_is_empty(&pool->using_block_list))
		{
			//获得最近使用的未用完的block
			block=get_first_dt(&pool->using_block_list,mempool_block,node);
			do
			{
				//如果block实际上已经耗尽，则将其加入到已用完链表
				if(0xffff==block->pos_clum[block->first_free_pos])
				{
					c_delete_node(&pool->using_block_list,&block->node);
					c_insert_in_head(&pool->used_block_list,&block->node);
					break;
				}
				//获得block的空闲链表中的第一个4k buf
				buf=(mp_4k_buf *)(block->start+4096*block->pos_clum[block->first_free_pos]);
				//将4k buf中的单元加入到cache中
				do
				{
					if(0xffff==buf->pos_clum[buf->first_free_pos])
					{
						block->first_free_pos=block->pos_clum[block->first_free_pos];
						block->free_count--;
						break;
					}
					pool->cache[++pool->cache_top]=((void*)buf)+pool->start_pos+pool->dt_size*buf->pos_clum[buf->first_free_pos];
					buf->first_free_pos=buf->pos_clum[buf->first_free_pos];
					buf->free_count--;
				}while(pool->cache_top<balance);
			}while(pool->cache_top<balance);
		}
		else if(!c_is_empty(&pool->free_block_list))//如果未使用完链表为空，从空闲链表中取
		{
			block=get_first_dt(&pool->free_block_list,mempool_block,node);
			c_delete_node(&pool->free_block_list,&block->node);
			c_insert_in_head(&pool->using_block_list,&block->node);
		}
		else //空闲链表也已经耗尽，则补充两个空闲block，允许第二个创建失败
		{
			if(NULL==get_new_block_4k(pool))
			{
				if(pool->flags&mem_flag_lock)
							arch_spin_unlock(&pool->lock);
				return NULL;
			}
			get_new_block_4k(pool);
		}
	}
	mem=pool->cache[pool->cache_top--];
	if(pool->flags&mem_flag_lock)
			arch_spin_unlock(&pool->lock);
	return mem;
}
void *get_mem_small(mempool *pool)
{
	if(unlikely(pool->blocksize!=4096))
		return NULL;
	return get_mem(pool);
}
void *get_mem_huge(mempool *pool)
{
	if(unlikely(pool->blocksize<=4096))
		return NULL;
	void *mem;
	if(NULL==(mem=get_mem(pool)))
		return NULL;
	return mem+sizeof(void*);
}
static inline void free_mem_small(void *mem)
{
	if(mem==NULL)
		return;
	mp_4k_buf * buf=(mp_4k_buf*)((unsigned long)mem&(~page_mask));
	mempool_block * block=buf->block;
	mempool *pool=block->pool;
	if(pool->flags&mem_flag_lock)
			arch_spin_unlock(&pool->lock);
	pool->cache[++pool->cache_top]=mem;
	if(pool->cache_top>=cache_size-1)
	{
		int balance=cache_size-cache_size/4;
		int offset=0;
		void *cache_mem;
		do
		{
			buf=(mp_4k_buf*)((unsigned long)(cache_mem=pool->cache[pool->cache_top--])&(~page_mask));
			block=buf->block;
			offset=(cache_mem-((void*)buf+pool->start_pos))/pool->dt_size;
			buf->pos_clum[offset]=buf->first_free_pos;
			buf->first_free_pos=offset;
			buf->free_count++;
			if(buf->free_count==buf->all_count)
			{
				offset=((void*)buf-block->start)/4096;
				block->pos_clum[offset]=block->first_free_pos;
				block->first_free_pos=offset;
				block->free_count++;
				if(block->free_count==block->all_count)
				{
					c_delete_node(&pool->using_block_list,&block->node);
					c_insert_in_head(&pool->free_block_list,&block->node);
					if(pool->free_block_list.count>16)
					{
						block=get_last_dt(&pool->free_block_list,mempool_block,node);
						c_delete_node(&pool->free_block_list,&block->node);
						if((void*)block<block->buf_start||(void*)block>block->buf_start+pool->blocksize)
						{
							free(block->buf_start);
							free(block);
						}
						else
							free(block->buf_start);
					}
				}
			}
		}while(pool->cache_top>balance);
	}
}
void free_mem_gent(void *mem)
{
	if(mem==NULL)
		return;
}
#endif /* LOCAL_MEMPOOL_H_ */
