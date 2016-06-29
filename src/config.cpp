/*
      alloc and initialize config 
 */

#include "config.h"
#include <cassert>

//initialize all config
server_config * server_config_init()
{
    server_config * config= (server_config*)malloc(sizeof(server_config));
	assert(config!=NULL);
	memset(config,0,sizeof(*config));
	config->service_name=buffer_init();
	config->service_root_dir=buffer_init();
	config->mimetype_filepath=buffer_init();
	config->minihttpd_global_config_filepath =buffer_init();
	config->log_filename=buffer_init();
	config->version_info=buffer_init();
    config->table=NULL;
	config->max_read_idle_ts=MINIHTTPD_MAX_IDLE_READ_TIMEOUT;
	config->max_write_idle_ts=MINIHTTPD_MAX_IDLE_WRITE_TIMEOUT;
	return config;
	
}

// set config to default
void set_config_default(server_config*config)
{
    assert(config!=NULL);
	buffer_copy_string(config->service_name,MINIHTTPD_SERVICE_NAME);
	config->support_http_1_1=MINIHTTPD_SUPPORT_HTTP11;
	config->max_http_head_size=(1<<16)-1;
	config->listenint_port=80;  //we use http port
	config->max_listening_number=MINIHTTPD_LISTENING_MAX_NUMBER;
	buffer_copy_string(config->service_root_dir,MINIHTTPD_ROOT_DIR);
	buffer_copy_string(config->mimetype_filepath,MINIHTTPD_MIME_PATH);
	buffer_copy_string(config->log_filename,MINIHTTPD_DEFAULT_LOG_FILEPATH);
	buffer_copy_string(config->minihttpd_global_config_filepath,MINIHTTPD_CONFIG_PATH);
 	config->max_fd=MINIHTTP_MAX_FD;
	config->max_worker_number=MINIHTTPD_WORKER_NUMBER;
	buffer_copy_string(config->version_info,MINIHTTP_VERSION_INFO);	
}


/* free server_config  */
void server_config_free(server_config * config)
{
	assert(config!=NULL);
	buffer_free(config->service_name);
    buffer_free(config->version_info);
	buffer_free(config->service_root_dir);
	buffer_free(config->mimetype_filepath);
	buffer_free(config->minihttpd_global_config_filepath);   
	buffer_free(config->log_filename);
	if(config->table)  mime_table_free(config->table);
	free((void*)config);
}
