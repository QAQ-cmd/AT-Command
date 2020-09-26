/******************************************************************************
 * @brief        ATָ��ͨ��
 *
 * Copyright (c) 2020, <master_roger@sina.com>
 *
 * SPDX-License-Identifier: Apathe-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
 * 2020-01-02     Morro        ����
 ******************************************************************************/
 
#include "at_chat.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

//��ʱ�ж�
#define AT_IS_TIMEOUT(start, time) (at_get_ms() - (start) > (time))

/*ATCOMM work type -----------------------------------------------------------*/
#define AT_TYPE_WORK       0                             /*���� --------------*/
#define AT_TYPE_CMD        1                             /*��׼���� ----------*/  
#define AT_TYPE_MULTILINE  3                             /*�������� ----------*/
#define AT_TYPE_SINGLLINE  4                             /*�������� ----------*/

typedef int (*base_work)(at_obj_t *at, ...);

static void at_send_line(at_obj_t *at, const char *fmt, va_list args);

static const inline at_core_conf_t *__get_adapter(at_obj_t *at) 
{
    return &at->cfg;
}

static bool is_timeout(at_obj_t *at, unsigned int ms)
{
    return AT_IS_TIMEOUT(at->resp_timer, ms);
}
/*
 * @brief   ��������
 */
static void send_data(at_obj_t *at, const void *buf, unsigned int len)
{
    at->cfg.write(buf, len);
}

/*
 * @brief       ��ʽ����ӡ
 */
static void print(at_obj_t *at, const char *cmd, ...)
{
    va_list args;	
    va_start(args, cmd);
    at_send_line(at, cmd, args);
    va_end(args);	
}
/*
 * @brief   ��ȡ��ǰ���ݽ��ճ���
 */
static unsigned int get_recv_count(at_obj_t *at)
{
    return at->rcv_cnt;
}

/*
 * @brief   ��ȡ���ݻ�����
 */
static char *get_recv_buf(at_obj_t *at)
{
    return (char *)at->cfg.rcv_buf;
}

/*
 * @brief   ������ݻ�����
 */
static void recv_buf_clear(at_obj_t *at)
{
    at->rcv_cnt = 0;
}

/*ǰ������ִ�*/
static char *search_string(at_obj_t *at, const char *str)
{
    return strstr(get_recv_buf(at), str);
}

/*ǰ������ִ�*/
static bool at_isabort(at_obj_t *at)
{
	return at->cursor ? at->cursor->abort : 1;
}


/*
 * @brief  ATִ�лص�
 */
static void do_at_callbatk(at_obj_t *a, at_item_t *i, at_callbatk_t cb, at_return ret)
{
    at_response_t r;
    if (cb) {
        r.param   = i->param;
        r.recvbuf = get_recv_buf(a);
        r.recvcnt = get_recv_count(a);
        r.ret     = ret;       
        cb(&r);
    }
}

/*
 * @brief       AT����
 * @param[in]   cfg   - AT��Ӧ
 */
void at_core_init(at_obj_t *at, const at_core_conf_t cfg)
{
    at_env_t *e;
    at->cfg  = cfg;
    e = &at->env;    
    at->rcv_cnt = 0;
    
    e->is_timeout = is_timeout;
    e->printf  = print;
    e->recvbuf = get_recv_buf;
    e->recvclr = recv_buf_clear;
    e->recvlen = get_recv_count;
    e->find    = search_string;
    e->abort   = at_isabort;
}
/*�����ҵ������*/
static bool add_work(at_obj_t *at, void *params, void *info, int type)
{
    at_item_t *i;
    if (list_empty(&at->ls_idle))                       //�޿���at_item
        return NULL;
    i = list_first_entry(&at->ls_idle, at_item_t, node);//�ӿ�������ȡ����ҵ
    i->info  = (void *)info;
    i->param = (void *)params;
    i->state = AT_STATE_WAIT;
    i->type  = type;
    i->abort = 0;
    list_move_tail(&i->node, &at->ls_ready);            //���������
    return i != 0;    
}

/*
 * @brief  ִ������
 */
static int do_work_handler(at_obj_t *at)
{
    at_item_t *i = at->cursor;
    return ((int (*)(at_env_t *e))i->info)(i->param);
}

/*******************************************************************************
 * @brief       ͨ������ִ��
 * @param[in]   a - AT������
 * @return      0 - ���ֹ���,��0 - ��������
 ******************************************************************************/
static int do_cmd_handler(at_obj_t *a)
{
    at_item_t *i = a->cursor;
    at_env_t  *e = &a->env;
    const at_respond_t *c = (at_respond_t *)i->info;
    switch(e->state) {
    case 0:  /*����״̬ ------------------------------------------------------*/                              
        c->sender(e);
        e->state++;
        e->reset_timer(a);
        e->recvclr(a);
    break;
    case 1: /*����״̬ ------------------------------------------------------*/ 
        if (search_string(a, c->matcher)) {                      	
            do_at_callbatk(a, i, c->cb, AT_RET_OK);
            return true;
        } else if (search_string(a, "ERROR")) {    
            if (++e->i >= c->retry) {
                do_at_callbatk(a, i, c->cb, AT_RET_ERROR);
                return true;
            }
            e->state = 2;                             /*����֮����ʱһ��ʱ��*/                
            e->reset_timer(a);                        /*���ö�ʱ��*/
        } else if (e->is_timeout(a, c->timeout))  {   
            if (++e->i >= c->retry) {
                do_at_callbatk(a, i, c->cb, AT_RET_TIMEOUT);
                return true;
            }                
            e->state = 0;                             /*������һ״̬*/
        }
    break; 
    case 2:
        if (e->is_timeout(a, 500))
            e->state = 0;                             /*���س�ʼ״̬*/    
    break;
    default: 
        e->state = 0;
    }
    return false;
}

/*******************************************************************************
 * @brief       ��������
 * @param[in]   a - AT������
 * @return      0 - ���ֹ���,��0 - ��������
 ******************************************************************************/
static int send_signlline_handler(at_obj_t *a)
{            
    at_item_t *i = a->cursor;
    at_env_t  *e = &a->env;    
    const char *cmd  = (const char *)i->param;
    at_callbatk_t cb = (at_callbatk_t)i->info;
    
    switch(e->state) {
    case 0:  /*����״̬ ------------------------------------------------------*/                              
        e->printf(a, cmd);
        e->state++;
        e->reset_timer(a);
        e->recvclr(a);
    break;
    case 1: /*����״̬ ------------------------------------------------------*/ 
        if (search_string(a, "OK")) {                      	
            do_at_callbatk(a, i, cb, AT_RET_OK);
            return true;
        } else if (search_string(a, "ERROR")) {
            if (++e->i >= 3) {
                do_at_callbatk(a, i, cb, AT_RET_ERROR);
                return true;
            }
            e->state = 2;                             /*����֮����ʱһ��ʱ��*/                
            e->reset_timer(a);                        /*���ö�ʱ��*/
        } else if (e->is_timeout(a, 3000 + e->i * 2000))  {   
            if (++e->i >= 3) {
                do_at_callbatk(a, i, cb, AT_RET_TIMEOUT);
                return true;
            }                
            e->state = 0;                             /*������һ״̬*/
        }            
    break; 
    case 2:
        if (e->is_timeout(a, 500))
            e->state = 0;                             /*���س�ʼ״̬*/    
    break;
    default: 
        e->state = 0;
    }
    return false;
}
/*******************************************************************************
 * @brief       �����������
 * @param[in]   a - AT������
 * @return      0 - ���ֹ���,��0 - ��������
 ******************************************************************************/
static int send_multiline_handler(at_obj_t *a)
{            
    at_item_t *i = a->cursor;
    at_env_t  *e = &a->env;        
    const char **cmds = (const char **)i->param;
    at_callbatk_t cb  = (at_callbatk_t)i->info;
    switch(e->state) {
    case 0:
        if (cmds[e->i] == NULL) {                    /*����ִ�����*/
            do_at_callbatk(a, i, cb, AT_RET_OK);
            return true;
        }
        e->printf(a, "%s\r\n", cmds[e->i]);
        e->recvclr(a);                               /*�������*/
        e->reset_timer(a);
        e->state++;
    break;
    case 1:
        if (search_string(a, "OK")){         
            e->state = 0;
            e->i++;
            e->i     = 0;
        } else if (search_string(a, "ERROR")) {
            if (++e->j >= 3) {
                do_at_callbatk(a, i, cb, AT_RET_ERROR);
                return true;
            }
            e->state = 2;                             /*����֮����ʱһ��ʱ��*/                
            e->reset_timer(a);                        /*���ö�ʱ��*/            
        } else if (e->is_timeout(a, 3000)) {
            do_at_callbatk(a, i, cb, AT_RET_TIMEOUT);
            return true;
        }       
    break;
    default: 
        e->state = 0;    
    }
    return 0;
}

/*
 * @brief       ������
 * @param[in]   fmt    - ��ʽ�����
 * @param[in]   args   - �������б�
 */
static void at_send_line(at_obj_t *at, const char *fmt, va_list args)
{
    char buf[MAX_AT_CMD_LEN];
    int len;
    const at_core_conf_t *adt = __get_adapter(at);
    len = vsnprintf(buf, sizeof(buf), fmt, args);

    recv_buf_clear(at);     //��ս��ջ���
    send_data(at, buf, len);
    send_data(at, "\r\n", 2);
}
/*
 * @brief       urc ���������
 * @param[in]   urc
 * @return      none
 */
static void urc_handler_entry(at_obj_t *at, char *urc, unsigned int size)
{
    int i, n;
    utc_item_t *tbl = at->cfg.utc_tbl;
    for (i = 0; i < at->cfg.urc_tbl_count; i++){
    n = strlen(tbl->prefix);
    if (strncmp(urc, tbl->prefix, n) == 0)
        tbl[i].handler(urc, size);
    }
}

/*
 * @brief       urc ���մ���
 * @param[in]   buf  - ���ݻ�����
 * @return      none
 */
static void urc_recv_process(at_obj_t *at, char *buf, unsigned int size)
{
    char *urc_buf;	
    unsigned short urc_size;
    urc_buf  = (char *)at->cfg.urc_buf;
    urc_size = at->cfg.urc_bufsize;	
    if (size == 0 && at->urc_cnt > 0) {
        if (AT_IS_TIMEOUT(at->urc_timer, 2000)){
            urc_handler_entry(at, urc_buf, at->urc_cnt);
            at->rcv_cnt = 0;
        }
    } else {
        at->urc_timer = at_get_ms();
        while (size--) {
            if (*buf == '\n') {
                urc_buf[at->urc_cnt] = '\0';
                urc_handler_entry(at, urc_buf, at->urc_cnt);
            } else {
            urc_buf[at->urc_cnt++] = *buf++;
            if (at->urc_cnt >= urc_size)
              at->urc_cnt = 0;
            }
        }
    }
}

/*
 * @brief       ָ����Ӧ���մ���
 * @param[in]   buf  - 
 * @return      none
 */
static void resp_recv_process(at_obj_t *at, const char *buf, unsigned int size)
{
    char *rcv_buf;
    unsigned short rcv_size;	
    
    rcv_buf  = (char *)at->cfg.rcv_buf;
    rcv_size = at->cfg.rcv_bufsize;

    if (at->rcv_cnt + size >= rcv_size)         //�������
        at->rcv_cnt = 0;
    
    memcpy(rcv_buf + rcv_cnt, buf, size);
    at->rcv_cnt += size;
    rcv_buf[at->rcv_cnt] = '\0';

}

/*
 * @brief       ִ��AT��ҵ
 * @param[in]   a      - AT������
 * @param[in]   work   - AT��ҵ���
 * @param[in]   params - 
 */
bool at_do_work(at_obj_t *at, int (*work)(at_env_t *e), void *params)
{
    return add_work(at, params, (void *)work, AT_TYPE_WORK);
}

/*
 * @brief       ִ��ATָ��
 * @param[in]   a - AT������
 * @param[in]   cmd   - cmd����
 */
bool at_do_cmd(at_obj_t *at, void *params, const at_respond_t *cmd)
{
    return add_work(at, params, (void *)cmd, AT_TYPE_CMD);
}

/*
 * @brief       ���͵���AT����
 * @param[in]   at          - AT������
 * @param[in]   cb          - ִ�лص�
 * @param[in]   singlline   - ��������
 * @note        ������ִ�����֮ǰ,singlline����ʼ����Ч
 */
bool at_send_singlline(at_obj_t *at, at_callbatk_t cb, const char *singlline)
{
    return add_work(at, (void *)singlline, (void *)cb, AT_TYPE_SINGLLINE);
}

/*
 * @brief       ���Ͷ���AT����
 * @param[in]   at          - AT������
 * @param[in]   cb          - ִ�лص�
 * @param[in]   multiline   - ��������
 * @note        ������ִ�����֮ǰ,multiline
 */
bool at_send_multiline(at_obj_t *at, at_callbatk_t cb, const char **multiline)
{
    return add_work(at, multiline, (void *)cb, AT_TYPE_MULTILINE);    
}

/*
 * @brief       ǿ����ֹAT��ҵ
 */

void at_item_abort(at_item_t *i)
{
	i->abort = 1;
}

/*
 * @brief       ATæ�ж�
 * @return      true - ��ATָ�������������ִ����
 */
bool at_core_busy(at_obj_t *at)
{
    return !list_empty(&at->ls_ready);
}

/*******************************************************************************
 * @brief   AT��ҵ����
 ******************************************************************************/
static void at_work_manager(at_obj_t *at)
{     
    register at_item_t *cursor = at->cursor;
    at_env_t           *e      = &at->env;
    /*ͨ�ù��������� ---------------------------------------------------------*/
    static int (*const work_handler_table[])(at_obj_t *) = {
    	do_work_handler, 
        do_cmd_handler,
        send_signlline_handler,
        send_multiline_handler
    };       
    if (at->cursor == NULL) {    
        if (list_empty(&at->ls_ready))                   //������Ϊ��
            return;
        e->i     = 0; 
        e->j     = 0;
        e->state = 0;
        e->params = cursor->param;        
        e->recvclr(at);
        e->reset_timer(at);
        at->cursor = list_first_entry(&at->ls_ready, at_item_t, node);
    }
    /*����ִ�����,�������뵽���й����� ------------------------------------*/
    if (work_handler_table[cursor->type](at) || cursor->abort) {
    	list_move_tail(&at->cursor->node, &at->ls_idle);
		at->cursor = NULL;
    }
        
}
/*
 * @brief  AT��ѯ����
 */
void at_poll_task(at_obj_t *at)
{
    char rbuf[32];
    int read_size;
    read_size = __get_adapter(at)->read(rbuf, sizeof(rbuf));
    urc_recv_process(at, rbuf, read_size);
    resp_recv_process(at, rbuf, read_size);    
    at_work_manager(at);
}


