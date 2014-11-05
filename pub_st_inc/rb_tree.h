/*
 * rb_tree.h
 *
 *  Created on: 2014-9-11
 *      Author: liwei
 */

#ifndef RB_TREE_H_
#define RB_TREE_H_
#include "stack.h"
#include "local_mempool.h"
#define BLACK 0x01
#define RED  0x00

#define IS_RIGHT 0x02
#define IS_LEFT  0x00

#define USE_PRT_KEY 0x01 //key是指针指向的一块内存，释放节点时需要释放掉key，比较时用memcmp
#define FREE_BY_CALLBACK 0x02  //dt的内容在节点释放时需要调用回调函数进行释放
#define COPY_DATA 0x04		//不用此项时，dt创建时直接用‘=’操作获得dt，当dt是char、int、long、指针这样不超过sizeof（void*）的内容时可使用此项。使用此项时将分配指定长度的内存并复制源数据。
#define USE_MEMPOOL 0x08 //使用内存池来存放树节点
/////COPY_DATA: dt=malloc(size);memcpy(dt,idt,size);
/////!COPY_DATA:dt=idt;
/////FREE_BY_CALLBACK: free_func(dt);if(COPY_DATA) free(dt);
/////!FREE_BY_CALLBACK: if(COPY_DATA) free(dt);

#define malloc_node(tree)  ((tree->flags&USE_MEMPOOL)?(rb_node*)get_mem(tree->mp):(rb_node*)malloc(sizeof(rb_node)))
#define node_free(tree,node) (tree->flags&USE_MEMPOOL)?free_mem_small(node):free(node);
#define free_node(tree,dnode) \
	if((tree)->flags&USE_PRT_KEY) \
		free((dnode)->kv.key); \
	if((tree)->flags&FREE_BY_CALLBACK) \
		(tree)->del_func((dnode)->kv.dt); \
	if((tree)->flags&COPY_DATA) \
		free((dnode)->kv.dt); \
	(tree->flags&USE_MEMPOOL)?free_mem_small(dnode):free(dnode);

typedef struct _RB_KV
{
	void* key;
	void* dt;
	unsigned int key_len;
	unsigned int dt_len;
}rb_key_value;
typedef struct _RB_NODE
{
	rb_key_value kv;
	struct _RB_NODE * l_node;
	struct _RB_NODE * r_node;
	struct _RB_NODE * p_node;
	uint8_t  color;
}rb_node;
typedef struct _RB_TREE
{
	struct _RB_NODE *root_node;
	struct _RB_NODE *null_node;
	unsigned int node_count;
	char is_change;
	uint8_t  flags;
	void*(*del_func)(void*);
	mempool *mp;
}rb_tree;

int rb_tree_init(rb_tree *tree ,uint8_t flags,void*(*del_func)(void*))
{
	if(NULL==(tree->null_node= (rb_node*)malloc(sizeof(rb_node))))
		return -1;
	tree->null_node->color=BLACK;
	tree->root_node=tree->null_node;
	tree->node_count=0;
	tree->is_change=1;
	tree->flags=flags;
	tree->del_func=del_func;
	if(flags&USE_MEMPOOL)
	{
		if(NULL==(tree->mp=(mempool*)malloc(sizeof(mempool))))
			return -1;
		init_mempool(tree->mp,0,sizeof(rb_node),32);
	}
	return 0;
}
void delete_tree(rb_node * dnode,rb_tree *tree)
{
	if (dnode==tree->null_node)
			return;
	if (dnode->l_node!=tree->null_node)
			delete_tree(dnode->l_node,tree);
	if (dnode->r_node!=tree->null_node)
			delete_tree(dnode->r_node,tree);
	free_node(tree,dnode);
}
void rb_tree_destory(rb_tree *tree)
{
	delete_tree(tree->root_node,tree);
	free(tree->null_node);
	if(tree->flags&USE_MEMPOOL)
	{
		if(0==(destory_mempool(tree->mp)))
			free(tree->mp);
		else
			printf("BUG!!!!!\n");
	}
}
static inline int rb_cmp_node_key(rb_tree *tree,void* skey,unsigned int skey_len,void* dkey,unsigned int dkey_len)
{
	int ret=0;
	if(tree->flags&USE_PRT_KEY)
	{
		if(skey_len>dkey_len)
		{
			ret=memcmp(skey,dkey,dkey_len);
			if(ret==0)
				return -1;
			return ret;
		}
		else
		{
			ret=memcmp(skey,dkey,skey_len);
			if(ret==0)
			{
				if(skey_len==dkey_len)
					return 0;
				else
					return -1;
			}
			return ret;
		}
	}
	else
		return skey-dkey;
}
#define grandpa_node(node) (node)->p_node->p_node
#define unclde_node(node) ((node)->p_node->p_node->l_node==(node)->p_node?grandpa_node((node))->r_node:grandpa_node((node))->l_node)
#define brother_node(node,tree) (node)->p_node!=tree->null_node?((node)->p_node->l_node==(node)?(node)->p_node->r_node:(node)->p_node->l_node):tree->null_node

#define r_or_l(node) ((node)->p_node->l_node==(node) ?IS_LEFT:IS_RIGHT)
static inline void retore_l_n(rb_node *brn,rb_tree * tree)
{
	rb_node * r=brn->r_node;
	brn->r_node=r->l_node;
	if (r->l_node!=tree->null_node)
		r->l_node->p_node=brn;
	r->p_node=brn->p_node;
	if(brn->p_node==tree->null_node)
		tree->root_node=r;
	else if(IS_LEFT==r_or_l(brn))
		brn->p_node->l_node=r;
	else
		brn->p_node->r_node=r;
	r->l_node=brn;
	brn->p_node=r;
}
static inline void retore_r_n(rb_node *brn,rb_tree * tree)
{
	rb_node * l=brn->l_node;
	brn->l_node=l->r_node;
	if (l->r_node!=tree->null_node)
		l->r_node->p_node=brn;
	l->p_node=brn->p_node;
	if(brn->p_node==tree->null_node)
		tree->root_node=l;
	else if(IS_LEFT==r_or_l(brn))
		brn->p_node->l_node=l;
	else
		brn->p_node->r_node=l;
	l->r_node=brn;
	brn->p_node=l;
}
static inline int find_p(void* key,unsigned int key_len,rb_node ** dt,rb_tree*tree)
{
	rb_node *br_dt=tree->root_node,*br_dt_b=tree->null_node;
	int ret;
	while(br_dt!=tree->null_node)
	{
		ret=rb_cmp_node_key(tree,key,key_len,br_dt->kv.key,br_dt->kv.key_len);
		if(ret==0)
		{
			(*dt)=br_dt;
			return 0;
		}
		else
		{
			br_dt_b=br_dt;
			if(ret>0)
				br_dt=br_dt->r_node;
			else
				br_dt=br_dt->l_node;
		}
	}
	*dt=br_dt_b;
	return -1;
}
static inline void rb_insert_fix(rb_node * brn,rb_tree *tree)
{
	if (tree->root_node==tree->null_node||brn==tree->root_node)
	{
		brn->color=BLACK;
		tree->root_node=brn;
		brn->p_node=tree->null_node;
		return;
	}
	if (brn->p_node->color==BLACK)
		return;
	if (!brn->p_node->color&&(unclde_node(brn)!=tree->null_node?unclde_node(brn)->color==RED:0))
	{
		brn->p_node->color=BLACK;
		(unclde_node(brn))->color=BLACK;
		grandpa_node(brn)->color=RED;
		brn=grandpa_node(brn);
		rb_insert_fix(brn,tree);
		return;
	}
	if (brn->p_node->p_node->l_node==brn->p_node&&brn->p_node->r_node==brn)
	{
		retore_l_n(brn->p_node,tree);
		brn=brn->l_node;
	}
	else if (brn->p_node->p_node->r_node==brn->p_node&&brn->p_node->l_node==brn)
	{
		retore_r_n(brn->p_node,tree);
		brn=brn->r_node;
	}
	grandpa_node(brn)->color=RED;
	brn->p_node->color=BLACK;
	if(brn->p_node->p_node->l_node==brn->p_node)
		retore_r_n(brn->p_node->p_node,tree);
	else
		retore_l_n(brn->p_node->p_node,tree);
}
static inline void rb_delete_fix(rb_node *brn,rb_tree *tree)
{
	rb_node * br_bro;
	while(brn!=tree->root_node&&brn->color==BLACK)
	{
		if (r_or_l(brn)==IS_LEFT)
		{
			br_bro=brn->p_node->r_node;
			if (br_bro->color==RED)
			{
				br_bro->color=BLACK;
				brn->p_node->color=RED;
				retore_l_n(brn->p_node,tree);
				br_bro=brn->p_node->r_node;
			}
			if (br_bro->l_node->color==BLACK&&br_bro->r_node->color==BLACK)
			{
				br_bro->color=RED;
				brn=brn->p_node;
			}
			else if (br_bro->r_node->color==BLACK)
			{
				br_bro->l_node->color=BLACK;
				br_bro->color=RED;
				retore_r_n(br_bro,tree);
				br_bro=brn->p_node->r_node;
			}
			else
			{
				br_bro->color=brn->p_node->color;
				brn->p_node->color=BLACK;
				br_bro->r_node->color=BLACK;
				retore_l_n(brn->p_node,tree);
				brn=tree->root_node;
				return;
			}
		}
		else
		{
			br_bro=brn->p_node->l_node;
			if (br_bro->color==RED)
			{
				br_bro->color=BLACK;
				brn->p_node->color=RED;
				retore_r_n(brn->p_node,tree);
				br_bro=brn->p_node->l_node;
			}
			if (br_bro->r_node->color==BLACK&&br_bro->l_node->color==BLACK)
			{
				br_bro->color=RED;
				brn=brn->p_node;
			}
			else if (br_bro->l_node->color==BLACK)
			{
				br_bro->r_node->color=BLACK;
				br_bro->color=RED;
				retore_l_n(br_bro,tree);
				br_bro=brn->p_node->l_node;
			}
			else
			{
				br_bro->color=brn->p_node->color;
				brn->p_node->color=BLACK;
				br_bro->l_node->color=BLACK;
				retore_r_n(brn->p_node,tree);
				brn=tree->root_node;
				return;
			}
		}
	}
	brn->color=BLACK;
}

static inline void*  rb_find(void *key,size_t key_len,rb_tree* tree)
{
	rb_node * brn;
	return find_p(key,key_len,&brn,tree)?brn->kv.dt:NULL;
}
static inline int rb_find1(void * key,size_t key_len,void ** pdt,rb_tree *tree)
{
	rb_node * brn;
	if(!find_p(key,key_len,&brn,tree))
		return -1;
	*pdt=brn->kv.dt;
	return 0;
}
static inline int rb_insert(rb_tree *tree,void * key,size_t key_len,void *dt,size_t dt_len)
{
	rb_node *br_dt=tree->null_node,*br_node_i;
	if (0==find_p(key,key_len,&br_dt,tree))
		return -1;
	if(NULL==(br_node_i=malloc_node(tree)))
		goto malloc_node_failed;
	if(tree->flags&USE_PRT_KEY)
	{
		if(NULL==(br_node_i->kv.key=malloc(key_len)))
			goto malloc_key_failed;
		memcpy(br_node_i->kv.key,key,key_len);
	}
	else
		br_node_i->kv.key=key;
	if(tree->flags&COPY_DATA)
	{
		if(NULL==(br_node_i->kv.dt=malloc(dt_len)))
			goto malloc_dt_failed;
		memcpy(br_node_i->kv.dt,dt,dt_len);
	}
	else
		br_node_i->kv.dt=dt;
	br_node_i->color=RED;
	br_node_i->l_node=br_node_i->r_node=tree->null_node;
	br_node_i->p_node=br_dt;
	if (br_dt!=tree->null_node)
	{
		if (br_dt->kv.key>key)
			br_dt->l_node=br_node_i;
		else
			br_dt->r_node=br_node_i;
	}
	rb_insert_fix(br_node_i,tree);
	tree->node_count++;
	//tree->is_change=true;
	return 0;
malloc_dt_failed:
	if(tree->flags&USE_PRT_KEY)
		free(br_node_i->kv.key);
malloc_key_failed:
	node_free(tree,br_node_i);
malloc_node_failed:
	return -1;
}
static inline int rb_earse(rb_tree * tree,void * key,size_t key_len,void **dt)
{
	rb_node * br_d,*b_front,*br_dr;
	int dt_len=0;
	if (!find_p(key,key_len,&br_d,tree))
		return -1;
	b_front=br_d;
	if(NULL!=dt)
	{
		*dt=br_d->kv.dt;
		dt_len=br_d->kv.dt_len;
	}
	if (br_d->l_node!=tree->null_node)
	{
		b_front=br_d->l_node;
		while(b_front->r_node!=tree->null_node)
			b_front=b_front->r_node;
	}
	if (b_front->r_node!=tree->null_node)
		br_dr=b_front->r_node;
	else
		br_dr=b_front->l_node;
	br_dr->p_node=b_front->p_node;
	if(b_front->p_node==tree->null_node)
		tree->root_node=br_dr;
	else if(r_or_l(b_front)==IS_LEFT)
		b_front->p_node->l_node=br_dr;
	else
		b_front->p_node->r_node=br_dr;
	if(b_front!=br_d)
		br_d->kv=b_front->kv;
	if (b_front->color==BLACK)
		rb_delete_fix(br_dr,tree);
	free_node(tree,b_front);
	//tree->is_change=true;
	return dt_len;
}
static inline int rb_updatedata(rb_key_value *kv,rb_tree * tree,void * dt ,size_t dtlen)
{
	if(tree->flags&COPY_DATA)
	{
		void * tmp;
		if(NULL==(tmp=malloc(dtlen)))
				return -1;
		if(tree->flags&FREE_BY_CALLBACK)
			tree->del_func(kv->dt);
		else
			free(kv->dt);
		kv->dt=tmp;
		memcpy(kv->dt,dt,dtlen);
		kv->dt_len=dtlen;
		return 0;
	}
	else
	{
		if(tree->flags&FREE_BY_CALLBACK)
			tree->del_func(kv->dt);
		kv->dt=dt;
		kv->dt_len=dtlen;
		return 0;
	}
}
typedef struct _rb_itor
{
	rb_node * right_pos;
	rb_tree * tree;
	db_stack stack;
}rb_itor;
static inline int init_rbitor_by_key(rb_itor * itor,rb_tree * tree,void * key,size_t key_len)
{
	rb_node *br_dt_b=tree->null_node;
	db_stack_init(&itor->stack,LEN_STATBLE,sizeof(rb_node *));
	itor->right_pos=tree->root_node;
	itor->tree=tree;
	int ret=0;
	while(itor->right_pos!=tree->null_node)
	{
		ret=rb_cmp_node_key(tree,key,key_len,br_dt_b->kv.key,br_dt_b->kv.key_len);
		if(ret==0)
			return 0;
		else
		{
			br_dt_b=itor->right_pos;
			if(ret>0)
			{
				st_push(&itor->stack,&br_dt_b,sizeof(br_dt_b));
				br_dt_b=br_dt_b->l_node;
			}
			else
			{
				st_pop(&itor->stack,NULL);
				br_dt_b=br_dt_b->r_node;
			}
		}
	}
	db_stack_destory(&itor->stack);
	return -1;
}
static inline int init_rbitor_at_begin(rb_itor * itor,rb_tree * tree)
{
	db_stack_init(&itor->stack,LEN_STATBLE,sizeof(rb_node *));
	itor->right_pos=tree->root_node;
	itor->tree=tree;
	if(tree->null_node==(itor->right_pos=tree->root_node))
		return 0;
	while(itor->right_pos!=tree->null_node)
	{
		st_push(&itor->stack,&itor->right_pos,sizeof(itor->right_pos));
		itor->right_pos=itor->right_pos->l_node;
	}
	st_pop(&itor->stack,&itor->right_pos);
	return 0;
}
static inline int rbitor_first(rb_itor * itor,rb_key_value **kv)
{
	if(itor->right_pos!=NULL)
	{
		*kv=&itor->right_pos->kv;
		return 0;
	}
	return -1;
}
static inline int rbitor_get_next(rb_itor * itor,rb_key_value **kv)
{
	if(itor->right_pos==itor->tree->null_node)
		return -1;
	if(itor->right_pos->r_node!=itor->tree->null_node)
	{
		itor->right_pos=itor->right_pos->r_node;
		while(itor->right_pos!=itor->tree->null_node)
		{
			st_push(&itor->stack,&itor->right_pos,sizeof(itor->right_pos));
			itor->right_pos=itor->right_pos->l_node;
		}
		st_pop(&itor->stack,&itor->right_pos);
	}
	else if(0==st_pop(&itor->stack,&itor->right_pos))
		return -1;
	*kv=&itor->right_pos->kv;
	return 0;
}
#endif /* RB_TREE_H_ */
