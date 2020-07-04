/******************************************************************************
 * @brief    AT ͨ�Ź���(��OS�汾)
 *
 * Copyright (c) 2019, <master_roger@sina.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
* 2016-01-22     Morro        Initial version. 
* 2018-02-11     Morro        ʹ����ʽ���й���AT��ҵ
* 2020-05-21     Morro        ֧��at_core����
 ******************************************************************************/
#ifndef _ATCHAT_H_
#define _ATCHAT_H_

#include "at_util.h"
#include <list.h>
#include <stdbool.h>

#define MAX_AT_CMD_LEN          128

struct at_core;

/*urc������ -----------------------------------------------------------------*/
typedef struct {
    const char *prefix;  //��Ҫƥ���ͷ��
    void (*handler)(char *recvbuf, int size); 
}utc_item_t;

typedef struct {
    unsigned int (*write)(const void *buf, unsigned int len);   /*���ͽӿ�*/
    unsigned int (*read)(void *buf, unsigned int len);          /*���սӿ�*/
    void (*lock)(void);                                         /*����,���OS*/
    void (*unlock)(void);                                       /*����,���OS*/
    /*Events -----------------------------------------------------------------*/
    void         (*before_at)(void);                            /*��ʼִ��AT*/
    void         (*after_at)(void);
    void         (*error)(void);
    utc_item_t    *utc_tbl;                                     /*utc ��*/
    unsigned char *urc_buf;                                     /*urc���ջ�����*/
    unsigned char *rcv_buf;
    unsigned short urc_tbl_count;
    unsigned short urc_bufsize;                                 /*urc��������С*/
    unsigned short rcv_bufsize;                                 /*���ջ�����*/
}at_core_conf_t;

/*AT��ҵ���л���*/
typedef struct {
    int         i,j,state;   
    void        *params;
    void        (*reset_timer)(struct at_core *ac);
    bool        (*is_timeout)(struct at_core *ac, unsigned int ms); /*ʱ�����ж�*/    
    void        (*printf)(struct at_core *ac, const char *fmt, ...);
    char *      (*find)(struct at_core *ac, const char *expect);
    char *      (*recvbuf)(struct at_core *ac);                 /*ָ����ջ�����*/
    unsigned int(*recvlen)(struct at_core *ac);                 /*�������ܳ���*/
    void        (*recvclr)(struct at_core *ac);                 /*��ս��ջ�����*/
    bool        (*abort)(struct at_core *ac);                   /*��ִֹ��*/
}at_env_t;

/*AT������Ӧ��*/
typedef enum {
    AT_RET_OK = 0,                                             /*ִ�гɹ�*/
    AT_RET_ERROR,                                              /*ִ�д���*/
    AT_RET_TIMEOUT,                                            /*��Ӧ��ʱ*/
    AT_RET_ABORT,                                              /*ǿ����ֹ*/
}at_return;

/*AT��Ӧ */
typedef struct {
    void           *param;
    char           *recvbuf;
    unsigned short  recvcnt;
    at_return       ret;
}at_response_t;

typedef void (*at_callback_t)(at_response_t *r);

/*AT״̬ */
typedef enum {
    AT_STATE_IDLE,                                             /*����״̬*/
    AT_STATE_WAIT,                                             /*�ȴ�ִ��*/
    AT_STATE_EXEC,                                             /*����ִ��*/
}at_work_state;

/*AT��ҵ��*/
typedef struct {
    at_work_state state : 3;
    unsigned char type  : 3;
    unsigned char abort : 1;
    void          *param;
    void          *info;
    struct list_head node;
}at_item_t;

/*AT������ ------------------------------------------------------------------*/
typedef struct at_core{
    at_core_conf_t          cfg;
    at_env_t                env;
    at_item_t               tbl[10];
    at_item_t               *cursor;
    struct list_head        ls_ready, ls_idle;               /*����,������ҵ��*/
    unsigned int            resp_timer;
    unsigned int            urc_timer;
    at_return               ret;
    //urc���ռ���, ������Ӧ���ռ�����
    unsigned short          urc_cnt, rcv_cnt;
    unsigned char           suspend: 1;
}at_core_t;

typedef struct {
    void (*sender)(at_env_t *e);                            /*�Զ��巢���� */
    const char *matcher;                                    /*����ƥ�䴮 */
    at_callback_t  cb;                                      /*��Ӧ���� */
    unsigned char  retry;                                   /*�������Դ��� */
    unsigned short timeout;                                 /*���ʱʱ�� */
}at_cmd_t;

void at_core_init(at_core_t *ac, const at_core_conf_t cfg);

/*���͵���AT����*/
bool at_send_singlline(at_core_t *ac, at_callback_t cb, const char *singlline);
/*���Ͷ���AT����*/
bool at_send_multiline(at_core_t *ac, at_callback_t cb, const char **multiline);
/*ִ��AT����*/
bool at_do_cmd(at_core_t *ac, void *params, const at_cmd_t *cmd);
/*�Զ���AT��ҵ*/
bool at_do_work(at_core_t *ac, int (*work)(at_env_t *e), void *params);

void at_item_abort(at_item_t *it);                       /*��ֹ��ǰ��ҵ*/

bool at_core_busy(at_core_t *ac);

void at_suspend(at_core_t *ac);

void at_resume(at_core_t *ac);


void at_poll_task(at_core_t *ac);


#endif
