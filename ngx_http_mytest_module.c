#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
static ngx_int_t mytest_subrequest_post_handler(ngx_http_request_t *r, void *data, ngx_int_t rc);
static ngx_int_t ngx_http_mytest_handler(ngx_http_request_t *r);
static void mytest_post_handler(ngx_http_request_t *r);
static char* ngx_conf_set_echo(ngx_conf_t *cf, ngx_command_t *cmd, void* conf);

typedef struct {
	ngx_str_t stock[6];
	ngx_buf_t *temp;
} ngx_http_mytest_ctx_t;

static ngx_command_t ngx_http_mytest_commands[] = {
    { ngx_string("mytest"),      /* 设定echo的handler */
      NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
      &ngx_conf_set_echo,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    ngx_null_command
};
static ngx_http_module_t ngx_http_mytest_module_ctx = {  
    NULL,                       /* preconfiguration */
    //ngx_http_echo_filter_init,                       /* postconfiguration */
    NULL,
    NULL,                       /* create main configuration */
    NULL,                       /* init main configuration */
    
    NULL,                       /* create server configuration */
    NULL,                       /* merge server configuration */

    NULL, /* create location configuration */
    NULL   /* merge location configuration */
};
ngx_module_t ngx_http_mytest_module = {
    NGX_MODULE_V1,
    &ngx_http_mytest_module_ctx,      /* 模块的上下文 */ 
    ngx_http_mytest_commands,        /* 模块的指令 */
    NGX_HTTP_MODULE,                /* 模块的类型 */
    NULL,                           /* 初始化master */
    NULL,                           /* 初始化模块 */
    NULL,                           /* 初始化process*/
    NULL,                           /* 初始化线程 */
    NULL,                           /* 退出线程 */
    NULL,                           /* 退出process */
    NULL,                           /* 退出master */
    NGX_MODULE_V1_PADDING
};


static char* 
ngx_conf_set_echo(ngx_conf_t *cf, ngx_command_t *cmd, void* conf)
{
    ngx_http_core_loc_conf_t*   clcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "ngx_conf_set_echo called - [rainx]");

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_mytest_handler; //设置handler

    return NGX_CONF_OK;
}

//子请求接收到响应的处理
static ngx_int_t mytest_subrequest_post_handler(ngx_http_request_t *r, void *data, ngx_int_t rc) {
	ngx_http_request_t *pr = r->parent;
	ngx_http_mytest_ctx_t *myctx = ngx_http_get_module_ctx(pr, ngx_http_mytest_module);
	pr->headers_out.status = r->headers_out.status;
	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[error] sub method_name is : %V",  &r->method_name);
	if(r->headers_out.status == NGX_HTTP_OK)
	{
		//int flag = 0;
		myctx->temp = &r->upstream->buffer;
		ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[error] sub method_name is : %V",  &r->method_name);
		/**ngx_buf_t *pRecvBuf = &r->upstream->buffer;
		for(; pRecvBuf->pos != pRecvBuf->last; pRecvBuf->pos++)
		{
			
			
			if(*pRecvBuf->pos == ',' || *pRecvBuf->pos == '\"')
			{
				if(flag > 0)
					myctx->stock[flag-1].len = pRecvBuf->pos - myctx->stock[flag-1].data;
				flag++;
				myctx->stock[flag-1].data = pRecvBuf->pos + 1;
			}
			if(flag > 6)
				break;
			
		}**/
	}
	pr->write_event_handler = mytest_post_handler;
	return NGX_OK;
}
//父请求的处理
static void mytest_post_handler(ngx_http_request_t *r) {
	if(r->headers_out.status != NGX_HTTP_OK)
	{
		ngx_http_finalize_request(r, r->headers_out.status);
		return ;
	}
	ngx_http_mytest_ctx_t *myctx = ngx_http_get_module_ctx(r, ngx_http_mytest_module);

	//ngx_str_t output_format = ngx_string("stock[%V], Today current price: %V, volumn: %V");
	//int bodylen = output_format.len + myctx->stock[0].len + myctx->stock[1].len + myctx->stock[4].len - 6;
	int bodylen = myctx->temp->last - myctx->temp->pos;

	r->headers_out.content_length_n = bodylen; //content_length
	ngx_buf_t *b = ngx_create_temp_buf(r->pool, bodylen);
	//ngx_snprintf(b->pos, bodylen, (char*)output_format.data, &myctx->stock[0], &myctx->stock[1], &myctx->stock[4]);
	//b->last = b->pos + bodylen;
	b->last = ngx_cpymem(b->pos, myctx->temp, bodylen);

	b->last_buf = 1;

	ngx_chain_t out;
	out.buf = b;
	out.next = NULL;
	static ngx_str_t type = ngx_string("text/plain");
	r->headers_out.content_type = type; //content_type
	r->headers_out.status = NGX_HTTP_OK;

	r->connection->buffered |= NGX_HTTP_WRITE_BUFFERED;
	ngx_int_t ret = ngx_http_send_header(r); //sent head
	ret = ngx_http_output_filter(r, &out);   //sent body
	ngx_http_finalize_request(r, ret);
}

static ngx_int_t ngx_http_mytest_handler(ngx_http_request_t *r) {
	ngx_http_mytest_ctx_t *myctx = ngx_http_get_module_ctx(r, ngx_http_mytest_module);
	if(myctx == NULL)
	{
		myctx = ngx_palloc(r->pool, sizeof(ngx_http_mytest_ctx_t));
		if(myctx == NULL)
			return NGX_ERROR;
		ngx_http_set_ctx(r, myctx, ngx_http_mytest_module);
	}
	ngx_http_post_subrequest_t *psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
	if(psr == NULL)
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	psr->handler = mytest_subrequest_post_handler;
	psr->data = myctx;

	/**
	ngx_str_t sub_prefix = ngx_string("/list=");
	ngx_str_t sub_location;
	sub_location.len = sub_prefix.len + r->args.len;
	sub_location.data = ngx_palloc(r->pool, sub_location.len);
	ngx_snprintf(sub_location.data, sub_location.len, "%V%V", &sub_prefix, &r->args);
	**/
    ngx_str_t name = ngx_string("helloHeaders");
    ngx_str_t value = ngx_string("abc");
	ngx_table_elt_t *h;
    h = ngx_list_push(&r->headers_in.headers);
    h->hash = 1;
    h->key.len = name.len;
    h->key.data = name.data;
    h->value.len = value.len;
    h->value.data = value.data;
	ngx_str_t sub_location = ngx_string("/list");
	ngx_http_request_t *sr;
	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[error] method_name is : %V",  &r->method_name);

	ngx_int_t rc = ngx_http_subrequest(r, &sub_location, NULL, &sr, psr, NGX_HTTP_SUBREQUEST_IN_MEMORY);
	if(rc != NGX_OK)
		return NGX_ERROR;
	return NGX_DONE;
}
