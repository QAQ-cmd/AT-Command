/**
  ******************************************************************************
  * @brief        AT命令通信管理(OS版本)
  *
  * Copyright (c) 2020, <morro_luo@163.com>
  *
  * SPDX-License-Identifier: Apache-2.0
  *
  * Change Logs: 
  * Date           Author       Notes 
  * 2020-01-02     Morro        Initial version. 
  * 2021-02-01     Morro        支持URC回调中接收数据.
  * 2021-02-05     Morro        1.修改struct at_obj,去除链表管理机制
  *                             2.删除 at_obj_destroy接口
  * 2021-03-21     Morro        删除at_obj中的at_work_ctx_t域,减少内存使用
  * 2021-04-08     Morro        解决重复释放信号量导致命令出现等待超时的问题
  ******************************************************************************
  */
#include "at.h"
#include "comdef.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

/** 
  * @brief    默认调试接口
  */
static void nop_dbg(const char *fmt, ...){}


/**
  * @brief    输出字符串
  */
static void put_string(at_obj_t *at, const char *s)
{
    while (*s != '\0')
        at->adap.write(s++, 1);
}


/**
  * @brief    输出字符串(带换行)
  */
static void put_line(at_obj_t *at, const char *s)
{
    put_string(at, s);
    put_string(at, "\r\n");    
    at->adap.debug("->\r\n%s\r\n", s);
}

//打印输出
static void at_print(struct at_work_ctx *e, const char *cmd, ...)
{
    va_list args;
    va_start(args, cmd);
    char buf[MAX_AT_CMD_LEN];
    vsnprintf(buf, sizeof(buf), cmd, args);
    put_line(e->at, buf);
    va_end(args);	
}

/**
  * @brief   清除数据缓冲区
  */
static void recvbuf_clr(struct at_work_ctx *e)
{
    e->at->rcv_cnt = 0;
}

//等待AT命令响应
static at_return wait_resp(at_obj_t *at, at_respond_t *r)
{        
    at->ret   = AT_RET_TIMEOUT;
    at->resp_timer = at_get_ms();    
    at->rcv_cnt = 0;                   //清空接收缓存
    at->resp  = r;
    at_sem_wait(at->completed, r->timeout);
    at->adap.debug("<-\r\n%s\r\n", r->recvbuf);
    at->resp = NULL;
    return at->ret;
}

/**
  * @brief       等待接收到指定串
  * @param[in]   resp    - 期待待接收串(如"OK",">")
  * @param[in]   timeout - 等待超时时间
  */
at_return wait_recv(struct at_work_ctx *e, const char *resp, 
                    unsigned int timeout)
{
    char buf[64];
    int cnt = 0, len;
    at_obj_t *at = e->at;    
    at_return ret = AT_RET_TIMEOUT;
    unsigned int timer = at_get_ms();
    while (at_get_ms() - timer < timeout) {
        len = at->adap.read(buf, sizeof(buf) - cnt);
        cnt += len;
        buf[cnt] = '\0';
        if (strstr(buf, resp)) {
            ret =  AT_RET_OK;
            break;
        } else if (strstr(buf, "ERROR")) {
            ret =  AT_RET_ERROR;
            break;
        }
        at_delay(10);
    }
    at->adap.debug("%s\r\n", buf);
    return ret;
}

/**
  * @brief       创建AT控制器
  * @param[in]   adap - AT接口适配器
  */
void at_obj_init(at_obj_t *at, const at_adapter_t *adap)
{
    at->adap    = *adap;
    at->rcv_cnt = 0;
    
    at->cmd_lock = ril_sem_new(1);
    at->completed = ril_sem_new(0);
   
    if (at->adap.debug == NULL)
        at->adap.debug = nop_dbg;
    
}

/**
  * @brief       执行命令
  * @param[in]   fmt    - 格式化输出
  * @param[in]   r      - 响应参数,如果填NULL, 默认返回OK表示成功,等待5s
  * @param[in]   args   - 如变参数列表
  */
at_return at_do_cmd(at_obj_t *at, at_respond_t *r, const char *cmd)
{
    at_return ret;
    char      defbuf[64];
    at_respond_t  default_resp = {"OK", defbuf, sizeof(defbuf), 5000};
    if (r == NULL) {
        r = &default_resp;                 //默认响应      
    }
    if (!at_sem_wait(at->cmd_lock, r->timeout)) {
        return AT_RET_TIMEOUT;    
    }
    at->busy  = true;
    
    while (at->urc_cnt) {
        at_delay(10);
    }
    put_line(at, cmd);
    ret = wait_resp(at, r); 
    at_sem_post(at->cmd_lock);
    at->busy  = false;
    return ret;    
}

/**
  * @brief       执行AT作业
  * @param[in]   at    - AT控制器
  * @param[in]   work  - 作业入口函数(类型 - int (*)(at_work_ctx_t *))
  * @param[in]   params- 作业参数
  * @return      依赖于work的返回值
  */
int at_do_work(at_obj_t *at, at_work work, void *params)
{
    at_work_ctx_t ctx;
    int ret;
    if (!at_sem_wait(at->cmd_lock, 150  * 1000)) {
        return AT_RET_TIMEOUT;
    }
    at->busy  = true;
    while (at->urc_cnt) {                            //等待URC处理完成
        at_delay(1);
    }
    //构造at_work_ctx_t
    ctx.params    = params;
    ctx.printf    = at_print;
    ctx.recvclr   = recvbuf_clr;
    ctx.read      = at->adap.read;
    ctx.write     = at->adap.write;
    ctx.wait_resp = wait_recv;  
    ctx.at        = at;
    at->dowork  = true;
    at->rcv_cnt = 0;
    ret = work(&ctx);
    at->dowork = false;
    at_sem_post(at->cmd_lock);
    at->busy  = false;
    return ret;
}

/**
  * @brief       分割响应行
  * @param[in]   recvbuf  - 接收缓冲区 
  * @param[out]  lines    - 响应行数组
  * @param[in]   separator- 分割符(, \n)
  * @return      行数
  */
int at_split_respond_lines(char *recvbuf, char *lines[], int count, char separator)
{
    char *s = recvbuf;
    size_t i = 0;      
    if (s == NULL || lines == NULL) 
        return 0;     
        
    lines[i++] = s;    
    while(*s && i < count) {       
        if (*s == ',') {
            *s = '\0';                                       
            lines[i++] = s + 1;                           /*指向下一个子串*/
        }
        s++;        
    }    
    return i;
}

/**
  * @brief       urc 处理总入口
  * @param[in]   urcline - URC行
  * @return      true - 正常识别并处理, false - 未识别URC
  */
static bool urc_handler_entry(at_obj_t *at, char *urcline, unsigned int size)
{
    int i;
    int ch = urcline[size - 1];
    at_urc_ctx_t context = {at->adap.read, urcline, at->adap.urc_bufsize, size};
    
    const utc_item_t *tbl = at->adap.utc_tbl;
    
    if (tbl == NULL)
        return true;
    
    for (i = 0; i < at->adap.urc_tbl_count; i++) {
        if (strstr(urcline, tbl->prefix)) {               /* 匹配前缀*/
            
            if (tbl->end_mark) {                          /* 匹配结束标记*/
                if (!strchr(tbl->end_mark, ch))
                    return false;
            } else if (!(ch == '\r' || ch == '\n'|| ch == '\0'))
                return false;
            
            at->adap.debug("<=\r\n%s\r\n", urcline);
            
            tbl->handler(&context);                       /* 递交到上层处理 */
            return true;
        }    
        tbl++;
    }
    return false;        
}

/**
  * @brief       urc 接收处理
  * @param[in]   ch  - 接收字符
  * @return      none
  */
static void urc_recv_process(at_obj_t *at, const char *buf, unsigned int size)
{
    register char *urc_buf;	
    int ch;
    urc_buf  = at->adap.urc_buf;
    
    //接收超时处理,默认MAX_URC_RECV_TIMEOUT
    if (at->urc_cnt > 0 && at_istimeout(at->urc_timer, MAX_URC_RECV_TIMEOUT)) {
        urc_buf[at->urc_cnt] = '\0';
        at->urc_cnt = 0;
        if (at->urc_cnt > 2)
            at->adap.debug("urc recv timeout=>%s\r\n", urc_buf);
    }
    
    while (size--) {
        at->urc_timer = at_get_ms();
        ch =  *buf++;
        urc_buf[at->urc_cnt++] = ch;
        
        if (strchr(SPEC_URC_END_MARKS, ch) || ch == '\0') {      //结束标记
            urc_buf[at->urc_cnt] = '\0';
            
            if (ch == '\r' || ch == '\n'|| ch == '\0') {         //检测到1行URC        
                if (at->urc_cnt > 2) {
                    if (!urc_handler_entry(at, urc_buf, at->urc_cnt) && !at->busy)   
                        at->adap.debug("%s\r\n", urc_buf);       //未识别到的URC
                }
                at->urc_cnt = 0;
            } else if (urc_handler_entry(at, urc_buf, at->urc_cnt)) {
                at->urc_cnt = 0;
            }
        } else if (at->urc_cnt >= at->adap.urc_bufsize)          //溢出处理
                at->urc_cnt = 0;
    }
}

/**
  * @brief       命令响应通知
  * @return      none
  */
static void resp_notification(at_obj_t *at, at_return ret)
{
    at->ret = ret;
    at_sem_post(at->completed);
}

/**
  * @brief       指令响应接收处理
  * @param[in]   buf  - 接收缓冲区
  * @param[in]   size - 缓冲区数据长度
  * @return      none
  */
static void resp_recv_process(at_obj_t *at, const char *buf, unsigned int size)
{
    char *rcv_buf;
    unsigned short rcv_size;	
    at_respond_t *resp = at->resp;
    
    if (resp == NULL)                                    //无命令请求
        return;
    
    if (size) {
        rcv_buf  = (char *)resp->recvbuf;
        rcv_size = resp->bufsize;

        if (at->rcv_cnt + size >= rcv_size) {             //接收溢出
            at->rcv_cnt = 0;
            at->adap.debug("Receive overflow:%s", rcv_buf);
        }
        /*将接收到的数据放至rcv_buf中 ---------------------------------------------*/
        memcpy(rcv_buf + at->rcv_cnt, buf, size);
        at->rcv_cnt += size;
        rcv_buf[at->rcv_cnt] = '\0';
        
        if (strstr(rcv_buf, resp->matcher)) {            //接收匹配
            resp_notification(at, AT_RET_OK);
            return;
        } else if (strstr(rcv_buf, "ERROR")) {
            resp_notification(at, AT_RET_ERROR);
            return;
        }
    } 
    
    if (at_istimeout(at->resp_timer, resp->timeout))    //接收超时
        resp_notification(at, AT_RET_TIMEOUT);
    else if (at->suspend)                                //强制终止
        resp_notification(at, AT_RET_ABORT);

}

/**
  * @brief       AT忙判断
  * @return      true - 有AT指令或者任务正在执行中
  */
bool at_obj_busy(at_obj_t *at)
{
    return !at->busy && at_istimeout(at->urc_timer, 2000);
}

/**
  * @brief       挂起AT作业
  * @return      none
  */
void at_suspend(at_obj_t *at)
{
    at->suspend = 1;
}

/**
  * @brief       恢复AT作业
  * @return      none
  */
void at_resume(at_obj_t *at)
{
    at->suspend = 0;
}

/**
  * @brief       AT处理
  * @return      none
  */
void at_process(at_obj_t *at)
{
    char c;
    unsigned int len;
    if (at->dowork)      /* 自定义命令处理 */
        return;
    do {
        len = at->adap.read(&c, 1);
        urc_recv_process(at, &c,len);
        resp_recv_process(at, &c, len);
    } while (len);
}
