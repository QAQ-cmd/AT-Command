# AT Command

#### 介绍
一种AT命令通信解析模块,支持裸机(at_chat)和OS版本(at)。适用于modem、WIFI模块、蓝牙通信。

#### 软件架构

- at_chat.c at_chat.h list.h

用于无OS版本，使用链式队列及异步回调方式处理AT命令收发，支持URC处理。
- at.c at.h at_util.h 

用于OS版本, 使用前需要根据at_util.h规定的操作系统相关的接口进行移植,如提供信号量操作、任务延时等操作。
#### 使用说明

##### at_chat 模块(无OS)


```

static at_obj_t at;          //定义AT控制器

const at_adapter_t adap = {  //AT适配器接口
	//适配GPRS模块的串口读写接口
	.write       = uart_write,
	.read        = uart_read
	...
};

```


3.  初始化AT控制器

```

at_obj_init(&at, &adap);

```


4.  将AT控制器放入任务中轮询

```
void main(void)
{
    /*do something ...*/
    while (1) {
        /*do something ...*/
        
        at_poll_task(&at);
    }
}

```


5.  发送单行命令


```
/**
 * @brief AT执行回调处理程序
 */
static void read_csq_callback(at_response_t *r)
{
	/*...*/
}
at_send_singlline(&at, read_csq_callback, "AT+CSQ");
```

##### at 模块(OS版本)


```

static at_obj_t at;          //定义AT控制器

static char urc_buf[128];    //URC主动上报缓冲区

utc_item_t utc_tbl[] = {     //定义URC表
	"+CSQ: ", csq_updated_handler
}

const at_adapter_t adap = {  //AT适配器接口
	.urc_buf     = urc_buf,
	.urc_bufsize = sizeof(urc_buf),
	.utc_tbl     = utc_tbl,
	.urc_tbl_count = sizeof(utc_tbl) / sizeof(utc_item_t),	
	
	//适配GPRS模块的串口读写接口
	.write       = uart_write,
	.read        = uart_read
};

```

3.  初始化AT控制器并创建AT线程

```

void at_thread(void)
{
	at_obj_create(&at, &adap);
    while (1) {        
        at_thread(&at);
    }
}

```


4.  使用例子
查询GPRS模块信号质量：
	=> AT+CSQ
	
	<= +CSQ: 24, 0
	<= OK
	
```
/* 
 * @brief    获取csq值
 */ 
bool read_csq_value(at_obj_t *at, int *rssi, int *error_rate)
{
	//接收缓冲区
	unsigned char recvbuf[32];
	//AT响应
	at_respond_t r = {"OK", recvbuf, sizeof(recvbuf), 3000};
	//
	if (at_do_cmd(at, &r, "AT+CSQ") != AT_RET_OK)
		return false;
	//提取出响应数据
	return (sscanf(recv, "%*[^+]+CSQ: %d,%d", rssi, error_rate) == 2);

}


```
