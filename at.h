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
 * 2021-02-01     Morro        ֧��URC�ص��н�������.
 * 2021-02-05     Morro        1.�޸�struct at_obj,ȥ������������
 *                             2.ɾ�� at_obj_destroy�ӿ�
 ******************************************************************************/

#ifndef _AT_H_
#define _AT_H_

#include "at_util.h"
#include <stdbool.h>

/* ������������ */ 
#define MAX_AT_CMD_LEN        128

/* ����urc���ճ�ʱʱ��*/  
#define MAX_URC_RECV_TIMEOUT  300      

/* ָ����URC ��������б� */
#define SPEC_URC_END_MARKS     ",\r\n"
     
struct at_obj;                                                /* AT����*/

/*AT������Ӧ�� ---------------------------------------------------------------*/
typedef enum {
    AT_RET_OK = 0,                                            /* ִ�гɹ�*/
    AT_RET_ERROR,                                             /* ִ�д���*/
    AT_RET_TIMEOUT,                                           /* ��Ӧ��ʱ*/
	AT_RET_ABORT,                                             /* δ֪����*/
}at_return;

/**
 * @brief URC ������(Context) ����
 */
typedef struct {
    /**
     * @brief   ���ݶ�ȡ�ӿ�
     * @params  buf   - ������
     * @params  len   - ��������С    
     */
    unsigned int (*read)(void *buf, unsigned int len);       
    char *buf;                                                /* ���ݻ����� */
    int bufsize;                                              /* ��������С */
    int recvlen;                                              /* �ѽ������ݳ���*/
} at_urc_ctx_t;

/*(Unsolicited Result Codes (URCs))������ ------------------------------------*/
typedef struct {
    /**
     * @brief urc ǰ׺(��"+CSQ: ")
     */    
    const char *prefix;
    /**
     * @brief urc ָ�������ַ����(�ο�DEF_URC_END_MARKS�б������ָ����Ĭ��"\n")
     * @note  
     */     
    const char *end_mark;
    /**
     * @brief       urc�������
     * @params      ctx - URC ������
     */
    void (*handler)(at_urc_ctx_t *ctx);
}utc_item_t;
    
/**AT�ӿ������� --------------------------------------------------------------*/
typedef struct {
    //����д����
    unsigned int (*write)(const void *buf, unsigned int len);
    //���ݶ�����
    unsigned int (*read)(void *buf, unsigned int len);         
    //���Դ�ӡ���,�������Ҫ����NULL
    void         (*debug)(const char *fmt, ...);  
    //utc ��������,�������Ҫ����NULL
	utc_item_t    *utc_tbl;
    //urc���ջ�����,�������Ҫ����NULL
	char          *urc_buf;
    //urc�������,�������Ҫ����0
	unsigned short urc_tbl_count;  
    //urc��������С,�������Ҫ����0
	unsigned short urc_bufsize;
}at_adapter_t;

/*AT��Ӧ���� -----------------------------------------------------------------*/
typedef struct {    
    const char    *matcher;                                   /* ����ƥ�䴮 */
    char          *recvbuf;                                   /* ���ջ����� */
    unsigned short bufsize;                                   /* ���������� */
    unsigned int   timeout;                                   /* ���ʱʱ�� */
}at_respond_t;

/** work context  ------------------------------------------------------------*/
/**
 * @brief AT��ҵ������(Work Context) ����
 */
typedef struct at_work_ctx {
    //��ҵ����,��at_do_work�ӿڴ���
	void          *params;                                     
    unsigned int (*write)(const void *buf, unsigned int len);
    unsigned int (*read)(void *buf, unsigned int len);
    
    //��ӡ���
	void         (*printf)(struct at_work_ctx *self, const char *fmt, ...);
    //��Ӧ�ȴ�
	at_return    (*wait_resp)(struct at_work_ctx *self, const char *prefix, 
                              unsigned int timeout);
    //������ջ���
    void         (*recvclr)(struct at_work_ctx *self); 
}at_work_ctx_t;

/*AT���� ---------------------------------------------------------------------*/
typedef struct at_obj {
	at_adapter_t            adap;                             /* �ӿ�������*/
    at_work_ctx_t           ctx;                              /* work context*/
	at_sem_t                cmd_lock;                         /* ������*/
	at_sem_t                completed;                        /* ��������ź�*/
    at_respond_t            *resp;                            /* ATӦ����Ϣ*/
	unsigned int            resp_timer;                       /* ��Ӧ���ն�ʱ��*/
	unsigned int            urc_timer;                        /* URC��ʱ�� */
	at_return               ret;                              /* ����ִ�н��*/
	//urc���ռ���, ������Ӧ���ռ�����
	unsigned short          urc_cnt, rcv_cnt;
	unsigned char           busy   : 1;
	unsigned char           suspend: 1;
    unsigned char           dowork : 1;
}at_obj_t;

typedef at_return (*at_work)(at_work_ctx_t *);

void at_obj_create(at_obj_t *at, const at_adapter_t *adap);    /* AT��ʼ��*/

void at_obj_destroy(at_obj_t *at);

bool at_obj_busy(at_obj_t *at);

void at_suspend(at_obj_t *at);                                 /* ����*/
 
void at_resume(at_obj_t *at);                                  /* �ָ�*/

at_return at_do_cmd(at_obj_t *at, at_respond_t *r, const char *cmd);

//��Ӧ�зָ��
int at_split_respond_lines(char *recvbuf, char *lines[], int count, char separator);

at_return at_do_work(at_obj_t *at, at_work work, void *params);/* �Զ���AT��ҵ*/

void at_process(at_obj_t *at);                                 /* AT���մ���*/
        
#endif
