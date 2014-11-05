/*
 * stack.h
 *
 *  Created on: 2014-9-12
 *      Author: liwei
 */

#ifndef STACK_H_
#define STACK_H_
#include "db_chain.h"
#include "public.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#define LEN_IN_DT 0x0000001
#define LEN_STATBLE 0x000002
#define ST_DEBUG
typedef struct _db_stack_node
{
	size_t size;
	chain_node node;
	void *top;
	void *dt;
}db_stack_node;
typedef struct _db_stack
{
	chain_head head;
	int flags;
	size_t dt_size;
}db_stack;
static inline void db_stack_init(db_stack * st,int flags,int max_dt_len)
{
	c_init_chain(&st->head);
	st->flags=flags;
	st->dt_size=max_dt_len;
}
static inline void db_stack_destory(db_stack * st)
{
	realase_chain(&st->head,db_stack_node,node,c_free);
}
static inline int st_push(db_stack * st,void *dt,size_t len)
{
	size_t dt_len;
	db_stack_node *node;
	if(st->flags&LEN_STATBLE)
		dt_len=st->dt_size;
	else if(st->flags&LEN_IN_DT)
	{
		if(st->dt_size<len)
			return -1;
		dt_len=len+sizeof(size_t);
	}
	if(unlikely(NULL==(node=get_last_dt(&st->head,db_stack_node,node)))||(node->size-(node->top-(void*)&node->dt))<dt_len)
	{
		if(NULL==(node=(db_stack_node*)malloc(st->dt_size<<6)))
				return -2;
		node->size=(st->dt_size<<6)-offsetof(db_stack_node,dt);
		node->top=(void*)&node->dt;
		c_insert_in_end(&st->head,&node->node);
	}
	if(st->flags&LEN_IN_DT)
	{
#ifdef ST_DEBUG
		printf("dt pos:%lu,len pos:%lu,len:%ld\n",(unsigned long)node->top,(unsigned long)node->top+len,dt_len);
#endif
		memcpy(node->top,dt,len);
		*(size_t*)(node->top+len)=len;
		node->top=node->top+len+sizeof(dt_len);
	}
	else
	{
		memcpy(node->top,dt,dt_len);
		node->top+=dt_len;
	}
	return 0;
}
static inline size_t st_get_top_size(db_stack * st)
{
	db_stack_node *node;
	if(unlikely(!(st->flags&LEN_IN_DT)))
		return st->dt_size;
	if(unlikely(NULL==(node=get_first_dt(&st->head,db_stack_node,node)))||node->top-(void*)&node->dt<sizeof(size_t))
		return 0;
	return *(size_t*)(node->top-sizeof(size_t));
}

//默认dt有足够的空间，如果不确定栈中数据长度，使用本方法前先调用size_t st_get_top_size(db_stack * st)方法获得长度
static inline size_t st_pop(db_stack * st,void *dt)
{
	db_stack_node *node;
	if(unlikely(NULL==(node=get_last_dt(&st->head,db_stack_node,node))))
		return 0;
	if((st->flags&LEN_IN_DT)?node->top-(void*)&node->dt<sizeof(size_t):node->top-(void*)&node->dt<=0)
	{
		db_stack_node *tmp;
		if(st->head.count==1)
				return 0;
		tmp=node;
		node=get_prev_dt(node,db_stack_node,node);
		c_delete_node(&st->head,&tmp->node);
		free(tmp);
	}
	if(st->flags&LEN_IN_DT)
	{
#ifdef ST_DEBUG
		printf("len pos:%lu,",(unsigned long)node->top-sizeof(size_t));
#endif
		size_t len=*(size_t*)(node->top-sizeof(size_t));
#ifdef ST_DEBUG
		printf("dt pos:%lu,len:%lu\n",(unsigned long)node->top-sizeof(size_t)-len,len);
#endif
		if(dt!=NULL)
			memcpy(dt,node->top-sizeof(size_t)-len,len);
		node->top-=(sizeof(size_t)+len);
		return len;
	}
	else
	{
		if(dt!=NULL)
			memcpy(dt,node->top-st->dt_size,st->dt_size);
		node->top-=st->dt_size;
		return st->dt_size;
	}

}
static inline int st_empty(db_stack * st)
{
	if(unlikely(c_is_empty(&st->head)))
		return 1;
	db_stack_node *node=get_last_dt(&st->head,db_stack_node,node);
	return node->top==(void*)&node->dt;
}
#endif /* STACK_H_ */
