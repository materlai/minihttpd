
/*
    minihttpd  user configuration file 

 */
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "types.h"
#include "buffer.h"
#include "key_map.h"

/* macro about default server fields */

#define MINIHTTPD_SERVICE_NAME                "minihttpd"
#define MINIHTTP_VERSION_INFO                 "v0.1"

#define MINIHTTPD_SUPPORT_HTTP11             1 //  support http1.1 by default
#define MINIHTTPD_LISTENING_PORT             80
#define MINIHTTPD_LISTENING_MAX_NUMBER       128*8
#define MINIHTTP_MAX_FD                      4096   //max file descriptor for worker process

#define MINIHTTPD_ROOT_DIR                   "/var/opt/minihttpd/minihttpd"
#define MINIHTTPD_MIME_PATH                   "/var/opt/minihttpd/mime_type.conf"
#define MINIHTTPD_DEFAULT_LOG_FILEPATH        "/var/opt/minihttpd/minihttpd.log"
#define MINIHTTPD_CONFIG_PATH                 "/var/opt/minihttpd/minihttpd.conf"
#define MINIHTTPD_WORKER_CONFIG_PATH          "/var/opt/minihttpd/minihttpd_worker"
#define MINIHTTPD_WORKER_NUMBER                0

/*server global server */
typedef struct{ 

	 /*field about service name */
	 buffer * service_name;
     /*field about protocol support */
     uint32_t support_http_1_1;
	 uint32_t  max_http_head_size;
	
    /*field about listening sokcet */
	 short listenint_port;
	 uint32_t max_listening_number;

	 /*configuration field about web http service */
     buffer * service_root_dir;
     buffer * mimetype_filepath;;
	 buffer * minihttpd_global_config_filepath;
	 /* max file descriptor number */
	 uint32_t max_fd;
	 /*configuration about worker process */
	 uint32_t  max_worker_number;

	 /*filed about about log file (contain warning/error messages for main process) */
	 buffer * log_filename;

	 /*minihttpd service version */
	 buffer* version_info;

	 /* mime tables  */
	 mime_table * table;

}server_config;


//initialize all config
server_config * server_config_init();

// set config to default
void set_config_default(server_config*config);

#endif 
