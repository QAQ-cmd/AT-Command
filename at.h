/******************************************************************************
 * @brief        AT����ͨ�Ź���(OS�汾)
 *
 * Copyright (c) 2020, <morro_luo@163.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
 * 2020-01-02     Morro        Initial version. 
 ******************************************************************************/

#ifndef _AT_H_
#define _AT_H_

#include "at_util.h"
#include "list.h"
#include <stdbool.h>

#define MAX_AT_CMD_LEN        128

struct at_obj;                                                  /*AT����*/

/*urc������ -----------------------------------------------------------------*/
typedef struct {
    const char *prefix;                                         //URCǰ׺
    void (*handler)(char *recvbuf, int size); 
}utc_item_t;
    
/*AT�ӿ������� ---------------------------------------------------------------*/
typedef struct {
    /*���ݶ�д�ӿ� -----------------------------------------------------------*/
    unsigned int (*read)(void *buf, unsigned int len);          
    unsigned int (*write)(const void *buf, unsigned int len);
    void         (*debug)(const char *fmt, ...);                 
	utc_item_t    *utc_tbl;                                     /* utc ��*/
	char          *urc_buf;                                     /* urc���ջ�����*/
	unsigned short urc_tbl_count;                               /* urc�������*/
	unsigned short urc_bufsize;                                 /* urc��������С*/
}at_adapter_t;

/*AT������Ӧ�� ---------------------------------------------------------------*/
typedef enum {
    AT_RET_OK = 0,                                              /*ִ�гɹ�*/
    AT_RET_ERROR,                                               /*ִ�д���*/
    AT_RET_TIMEOUT,                                             /*��Ӧ��ʱ*/
	AT_RET_ABORT,                                               /*δ֪����*/
}at_return;

/*AT��Ӧ���� -----------------------------------------------------------------*/
typedef struct {    
    const char    *matcher;                                     /*����ƥ�䴮*/
    char          *recvbuf;                                     /*���ջ�����*/
    unsigned short bufsize;                                     /*�����ճ���*/
    unsigned int   timeout;                                     /*���ʱʱ�� */
}at_respond_t;

/*AT��ҵ ---------------------------------------------------------------------*/
typedef struct at_work_env {
    struct at_obj *at;
	void          *params;                                     
    unsigned int (*write)(const void *buf, unsigned int len);
    unsigned int (*read)(void *buf, unsigned int len);
    
	void         (*printf)(struct at_obj *at, const char *frm, ...);
	at_return    (*wait_resp)(struct at_obj *at, const char *resp, unsigned int timeout);
    void         (*recvclr)(struct at_obj *at);                /*��ս��ջ�����*/
}at_work_env_t;

/*AT���� ---------------------------------------------------------------------*/
typedef struct at_obj {
    struct list_head        node;
	at_adapter_t            adap;   
    at_work_env_t           env;
	at_sem_t                cmd_lock;                           /*������*/
	at_sem_t                completed;                          /*��������*/
    at_respond_t            *resp;
	unsigned int            resp_timer;
	unsigned int            urc_timer;
	at_return               ret;
	//urc���ռ���, ������Ӧ���ռ�����
	unsigned short          urc_cnt, rcv_cnt;
	unsigned char           wait   : 1;
	unsigned char           suspend: 1;
    unsigned char           dowork : 1;
}at_obj_t;

typedef at_return (*at_work)(at_work_env_t *);

void at_obj_create(at_obj_t *at, const at_adapter_t *adap);    /*AT��ʼ��*/

void at_obj_destroy(at_obj_t *at);

bool at_obj_busy(at_obj_t *at);

void at_suspend(at_obj_t *at);                                 /*����*/
 
void at_resume(at_obj_t *at);                                  /*�ָ�*/

at_return at_do_cmd(at_obj_t *at, at_respond_t *r, const char *cmd);

int at_split_respond_lines(char *recvbuf, char *lines[], int count, char separator);

at_return at_do_work(at_obj_t *at, at_work work, void *params);/*ִ��AT��ҵ*/

void at_thread(void);                                          /*AT�߳�*/
        
#endif
