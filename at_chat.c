/*******************************************************************************
* @file		at_core.h
* @brief	AT command communications.
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
* 2020-01-02     roger.luo   4.0 os version
* 2020-05-21     roger.luo   5.0 ��OS�汾
*******************************************************************************/
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

typedef int (*base_work)(at_core_t *ac, ...);

static void at_send_line(at_core_t *ac, const char *fmt, va_list args);

static const inline at_core_conf_t *__get_adapter(at_core_t *ac) 
{
    return &ac->cfg;
}

static bool is_timeout(at_core_t *ac, unsigned int ms)
{
    return AT_IS_TIMEOUT(ac->resp_timer, ms);
}
/*
 * @brief   ��������
 */
static void send_data(at_core_t *ac, const void *buf, unsigned int len)
{
    ac->cfg.write(buf, len);
}

/*
 * @brief       ��ʽ����ӡ
 */
static void print(at_core_t *ac, const char *cmd, ...)
{
    va_list args;	
    va_start(args, cmd);
    at_send_line(ac, cmd, args);
    va_end(args);	
}
/*
 * @brief   ��ȡ��ǰ���ݽ��ճ���
 */
static unsigned int get_recv_count(at_core_t *ac)
{
    return ac->rcv_cnt;
}

/*
 * @brief   ��ȡ���ݻ�����
 */
static char *get_recv_buf(at_core_t *ac)
{
    return (char *)ac->cfg.rcv_buf;
}

/*
 * @brief   ������ݻ�����
 */
static void recv_buf_clear(at_core_t *ac)
{
    ac->rcv_cnt = 0;
}

/*ǰ������ִ�*/
static char *search_string(at_core_t *ac, const char *str)
{
    return strstr(get_recv_buf(ac), str);
}

/*ǰ������ִ�*/
static bool at_isabort(at_core_t *ac)
{
	return ac->cursor ? ac->cursor->abort : 1;
}


/*
 * @brief  ATִ�лص�
 */
static void do_at_callback(at_core_t *a, at_item_t *i, at_callback_t cb, at_return ret)
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
void at_core_init(at_core_t *ac, const at_core_conf_t cfg)
{
    at_env_t *e;
    ac->cfg  = cfg;
    e = &ac->env;    
    ac->rcv_cnt = 0;
    
    e->is_timeout = is_timeout;
    e->printf  = print;
    e->recvbuf = get_recv_buf;
    e->recvclr = recv_buf_clear;
    e->recvlen = get_recv_count;
    e->find    = search_string;
    e->abort   = at_isabort;
}
/*�����ҵ������*/
static bool add_work(at_core_t *ac, void *params, void *info, int type)
{
    at_item_t *i;
    ac->cfg.lock();
    if (list_empty(&ac->ls_idle))                       //�޿���at_item
        return NULL;
    i = list_first_entry(&ac->ls_idle, at_item_t, node);//�ӿ�������ȡ����ҵ
    i->info  = (void *)info;
    i->param = (void *)params;
    i->state = AT_STATE_WAIT;
    i->type  = type;
    i->abort = 0;
    list_move_tail(&i->node, &ac->ls_ready);            //���������
    ac->cfg.unlock();
    return i != 0;    
}

/*
 * @brief  ִ������
 */
static int do_work_handler(at_core_t *ac)
{
    at_item_t *i = ac->cursor;
    return ((int (*)(at_env_t *e))i->info)(i->param);
}

/*******************************************************************************
 * @brief       ͨ������ִ��
 * @param[in]   a - AT������
 * @return      0 - ���ֹ���,��0 - ��������
 ******************************************************************************/
static int do_cmd_handler(at_core_t *a)
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
            do_at_callback(a, i, c->cb, AT_RET_OK);
            return true;
        } else if (search_string(a, "ERROR")) {    
            if (++e->i >= c->retry) {
                do_at_callback(a, i, c->cb, AT_RET_ERROR);
                return true;
            }
            e->state = 2;                             /*����֮����ʱһ��ʱ��*/                
            e->reset_timer(a);                        /*���ö�ʱ��*/
        } else if (e->is_timeout(a, c->timeout))  {   
            if (++e->i >= c->retry) {
                do_at_callback(a, i, c->cb, AT_RET_TIMEOUT);
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
static int send_signlline_handler(at_core_t *a)
{            
    at_item_t *i = a->cursor;
    at_env_t  *e = &a->env;    
    const char *cmd  = (const char *)i->param;
    at_callback_t cb = (at_callback_t)i->info;
    
    switch(e->state) {
    case 0:  /*����״̬ ------------------------------------------------------*/                              
        e->printf(a, cmd);
        e->state++;
        e->reset_timer(a);
        e->recvclr(a);
    break;
    case 1: /*����״̬ ------------------------------------------------------*/ 
        if (search_string(a, "OK")) {                      	
            do_at_callback(a, i, cb, AT_RET_OK);
            return true;
        } else if (search_string(a, "ERROR")) {
            if (++e->i >= 3) {
                do_at_callback(a, i, cb, AT_RET_ERROR);
                return true;
            }
            e->state = 2;                             /*����֮����ʱһ��ʱ��*/                
            e->reset_timer(a);                        /*���ö�ʱ��*/
        } else if (e->is_timeout(a, 3000 + e->i * 2000))  {   
            if (++e->i >= 3) {
                do_at_callback(a, i, cb, AT_RET_TIMEOUT);
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
static int send_multiline_handler(at_core_t *a)
{            
    at_item_t *i = a->cursor;
    at_env_t  *e = &a->env;        
    const char **cmds = (const char **)i->param;
    at_callback_t cb  = (at_callback_t)i->info;
    switch(e->state) {
    case 0:
        if (cmds[e->i] == NULL) {                    /*����ִ�����*/
            do_at_callback(a, i, cb, AT_RET_OK);
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
                do_at_callback(a, i, cb, AT_RET_ERROR);
                return true;
            }
            e->state = 2;                             /*����֮����ʱһ��ʱ��*/                
            e->reset_timer(a);                        /*���ö�ʱ��*/            
        } else if (e->is_timeout(a, 3000)) {
            do_at_callback(a, i, cb, AT_RET_TIMEOUT);
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
static void at_send_line(at_core_t *ac, const char *fmt, va_list args)
{
    char buf[MAX_AT_CMD_LEN];
    int len;
    const at_core_conf_t *adt = __get_adapter(ac);
    len = vsnprintf(buf, sizeof(buf), fmt, args);

    recv_buf_clear(ac);     //��ս��ջ���
    send_data(ac, buf, len);
    send_data(ac, "\r\n", 2);
}
/*
 * @brief       urc ���������
 * @param[in]   urc
 * @return      none
 */
static void urc_handler_entry(at_core_t *ac, char *urc, unsigned int size)
{
    int i, n;
    utc_item_t *tbl = ac->cfg.utc_tbl;
    for (i = 0; i < ac->cfg.urc_tbl_count; i++){
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
static void urc_recv_process(at_core_t *ac, char *buf, unsigned int size)
{
    char *urc_buf;	
    unsigned short urc_size;
    urc_buf  = (char *)ac->cfg.urc_buf;
    urc_size = ac->cfg.urc_bufsize;	
    if (size == 0 && ac->urc_cnt > 0) {
        if (AT_IS_TIMEOUT(ac->urc_timer, 2000)){
            urc_handler_entry(ac, urc_buf, ac->urc_cnt);
            ac->rcv_cnt = 0;
        }
    } else {
        ac->urc_timer = at_get_ms();
        while (size--) {
            if (*buf == '\n') {
                urc_buf[ac->urc_cnt] = '\0';
                urc_handler_entry(ac, urc_buf, ac->urc_cnt);
            } else {
            urc_buf[ac->urc_cnt++] = *buf++;
            if (ac->urc_cnt >= urc_size)
              ac->urc_cnt = 0;
            }
        }
    }
}

/*
 * @brief       ָ����Ӧ���մ���
 * @param[in]   buf  - 
 * @return      none
 */
static void resp_recv_process(at_core_t *ac, const char *buf, unsigned int size)
{
    char *rcv_buf;
    unsigned short rcv_size;	
    
    rcv_buf  = (char *)ac->cfg.rcv_buf;
    rcv_size = ac->cfg.rcv_bufsize;

    if (ac->rcv_cnt + size >= rcv_size)         //�������
        ac->rcv_cnt = 0;
    
    memcpy(rcv_buf + rcv_cnt, buf, size);
    ac->rcv_cnt += size;
    rcv_buf[ac->rcv_cnt] = '\0';

}

/*
 * @brief       ִ��AT��ҵ
 * @param[in]   a      - AT������
 * @param[in]   work   - AT��ҵ���
 * @param[in]   params - 
 */
bool at_do_work(at_core_t *ac, int (*work)(at_env_t *e), void *params)
{
    return add_work(ac, params, (void *)work, AT_TYPE_WORK);
}

/*
 * @brief       ִ��ATָ��
 * @param[in]   a - AT������
 * @param[in]   cmd   - cmd����
 */
bool at_do_cmd(at_core_t *ac, void *params, const at_respond_t *cmd)
{
    return add_work(ac, params, (void *)cmd, AT_TYPE_CMD);
}

/*
 * @brief       ���͵���AT����
 * @param[in]   ac          - AT������
 * @param[in]   cb          - ִ�лص�
 * @param[in]   singlline   - ��������
 * @note        ������ִ�����֮ǰ,singlline����ʼ����Ч
 */
bool at_send_singlline(at_core_t *ac, at_callback_t cb, const char *singlline)
{
    return add_work(ac, (void *)singlline, (void *)cb, AT_TYPE_SINGLLINE);
}

/*
 * @brief       ���Ͷ���AT����
 * @param[in]   ac          - AT������
 * @param[in]   cb          - ִ�лص�
 * @param[in]   multiline   - ��������
 * @note        ������ִ�����֮ǰ,multiline
 */
bool at_send_multiline(at_core_t *ac, at_callback_t cb, const char **multiline)
{
    return add_work(ac, multiline, (void *)cb, AT_TYPE_MULTILINE);    
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
bool at_core_busy(at_core_t *ac)
{
    return !list_empty(&ac->ls_ready);
}

/*******************************************************************************
 * @brief   AT��ҵ����
 ******************************************************************************/
static void at_work_manager(at_core_t *ac)
{     
    register at_item_t *cursor = ac->cursor;
    at_env_t           *e      = &ac->env;
    /*ͨ�ù��������� ---------------------------------------------------------*/
    static int (*const work_handler_table[])(at_core_t *) = {
    	do_work_handler, 
        do_cmd_handler,
        send_signlline_handler,
        send_multiline_handler
    };       
    if (ac->cursor == NULL) {    
        if (list_empty(&ac->ls_ready))                   //������Ϊ��
            return;
        e->i     = 0; 
        e->j     = 0;
        e->state = 0;
        e->params = cursor->param;        
        e->recvclr(ac);
        e->reset_timer(ac);
        ac->cursor = list_first_entry(&ac->ls_ready, at_item_t, node);
    }
    /*����ִ�����,�������뵽���й����� ------------------------------------*/
    if (work_handler_table[cursor->type](ac) || cursor->abort) {
    	ac->cfg.lock();
    	list_move_tail(&ac->cursor->node, &ac->ls_idle);
		ac->cursor = NULL;
		ac->cfg.unlock();
    }
        
}
/*
 * @brief  AT��ѯ����
 */
void at_poll_task(at_core_t *ac)
{
    char rbuf[32];
    int read_size;
    read_size = __get_adapter(ac)->read(rbuf, sizeof(rbuf));
    urc_recv_process(ac, rbuf, read_size);
    resp_recv_process(ac, rbuf, read_size);    
    at_work_manager(ac);
}


