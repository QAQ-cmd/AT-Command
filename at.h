/*******************************************************************************
* @file		at.h
* @brief	AT����ͨ�Ź���
* 			
* @version	5.0
* @date		2020-05-11
* @author	roger.luo
*
* Change Logs: 
* Date           Author       Notes 
* 2016-01-22     roger.luo   Initial version. 
* 2017-05-21     roger.luo   1.1 ��������״̬����   
* 2018-02-11     roger.luo   3.0 
* 2020-01-02     roger.luo   4.0 ����os�汾
*******************************************************************************/
#ifndef _AT_H_
#define _AT_H_

#include "at_util.h"
#include "list.h"
#include <stdbool.h>

#define MAX_AT_CMD_LEN          64

struct at_obj;                                                  /*AT����*/

/*urc������ -----------------------------------------------------------------*/
typedef struct {
    const char *prefix;                                         //URCǰ׺
    void (*handler)(char *recvbuf, int size); 
}utc_item_t;
    
/*AT������ -------------------------------------------------------------------*/
typedef struct {
    /*���ݶ�д�ӿ� -----------------------------------------------------------*/
    unsigned int (*read)(void *buf, unsigned int len);          
    unsigned int (*write)(const void *buf, unsigned int len);
    void         (*debug)(const char *fmt, ...);
	utc_item_t    *utc_tbl;                                     /*utc ��*/
	char          *urc_buf;                                     /*urc���ջ�����*/
	unsigned short urc_tbl_count;
	unsigned short urc_bufsize;                                 /*urc��������С*/
}at_conf_t;

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
typedef struct at_work_env{   
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
	at_conf_t               cfg;   
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

typedef int (*at_work)(at_work_env_t *);

void at_obj_create(at_obj_t *at, const at_conf_t cfg);         /*AT��ʼ��*/

void at_obj_destroy(at_obj_t *at);

bool at_obj_busy(at_obj_t *at);

void at_suspend(at_obj_t *at);                                 /*����*/
 
void at_resume(at_obj_t *at);                                  /*�ָ�*/

at_return at_do_cmd(at_obj_t *at, at_respond_t *r, const char *cmd);

int at_split_respond_lines(char *recvbuf, char *lines[], int count);

int at_do_work(at_obj_t *at, at_work work, void *params);      /*ִ��AT��ҵ*/

void at_thread(void);                                          /*AT�߳�*/
        
#endif
