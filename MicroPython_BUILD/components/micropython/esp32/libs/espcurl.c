

#include "sdkconfig.h"

#if defined(CONFIG_MICROPY_USE_CURL) || defined(CONFIG_MICROPY_USE_SSH)

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "libs/espcurl.h"
#include "libs/libGSM.h"

#include "libssh2.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "mphalport.h"

#include "esp_wifi_types.h"
#include "tcpip_adapter.h"

#include "py/mpthread.h"

// Set default values for configuration variables
uint8_t curl_verbose = 1;        // show detailed info of what curl functions are doing
uint8_t curl_progress = 1;       // show progress during transfers
uint16_t curl_timeout = 60;      // curl operations timeout in seconds
uint32_t curl_maxbytes = 300000; // limit download length
uint8_t curl_initialized = 0;

#if CONFIG_SPIRAM_SUPPORT
int hdr_maxlen = 1024;
int body_maxlen = 4096;
#else
int hdr_maxlen = 512;
int body_maxlen = 1024;
#endif


//--------------------
void checkConnection()
{
    tcpip_adapter_ip_info_t info;
    tcpip_adapter_get_ip_info(WIFI_IF_STA, &info);
    if (info.ip.addr == 0) {
		#ifdef CONFIG_MICROPY_USE_GSM
    	if (ppposStatus() != GSM_STATE_CONNECTED) {
    		nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "No Internet connection"));
    	}
		#else
		nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "No Internet connection"));
		#endif
    }
}

#ifdef CONFIG_MICROPY_USE_CURL

static uint8_t curl_sim_fs = 0;

struct curl_Transfer {
	char *ptr;
	uint32_t len;
	uint32_t size;
	int status;
	uint8_t tofile;
	uint32_t maxlen;
	double lastruntime;
	FILE *file;
	CURL *curl;
};

struct curl_httppost *formpost = NULL;
struct curl_httppost *lastptr = NULL;


// Initialize the structure used in curlWrite callback
//-----------------------------------------------------------------------------------------------------------
static void init_curl_Transfer(CURL *curl, struct curl_Transfer *s, char *buf, uint16_t maxlen, FILE* file) {
    s->len = 0;
    s->size = 0;
    s->status = 0;
    s->maxlen = maxlen;
    s->lastruntime = 0;
    s->tofile = 0;
    s->file = file;
    s->ptr = buf;
    s->curl = curl;
    if (s->ptr) s->ptr[0] = '\0';
}

// Callback: Get response header or body to buffer or file
//--------------------------------------------------------------------------------
static size_t curlWrite(void *buffer, size_t size, size_t nmemb, void *userdata) {
	struct curl_Transfer *s = (struct curl_Transfer *)userdata;
	CURL *curl = s->curl;
	double curtime = 0;
	curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);

	if (!s->tofile) {
		// === Downloading to buffer ===
		char *buf = (char *)buffer;
		if (s->ptr) {
			if ((s->status == 0) && ((size*nmemb) > 0)) {
				for (int i=0; i<(size*nmemb); i++) {
					if (s->len < (s->maxlen-2)) {
						if (((buf[i] == 0x0a) || (buf[i] == 0x0d)) || ((buf[i] >= 0x20) && (buf[i] >= 0x20))) s->ptr[s->len] = buf[i];
						else s->ptr[s->len] = '.';
						s->len++;
						s->ptr[s->len] = '\0';
					}
					else {
						s->status = 1;
						break;
					}
				}
				if ((curl_progress) && ((curtime - s->lastruntime) > curl_progress)) {
					s->lastruntime = curtime;
					printf("* Download: received %d\r\n", s->len);
				}
			}
		}
		return size*nmemb;
	}
	else {
		// === Downloading to file ===
		size_t nwrite;

		if (curl_sim_fs) nwrite = size*nmemb;
		else {
			nwrite = fwrite(buffer, 1, size*nmemb, s->file);
			if (nwrite <= 0) {
				printf("* Download: Error writing to file %d\r\n", nwrite);
				return 0;
			}
		}

		s->len += nwrite;
		if ((curl_progress) && ((curtime - s->lastruntime) > curl_progress)) {
			s->lastruntime = curtime;
			printf("* Download: received %d\r\n", s->len);
		}

		return nwrite;
	}
}

// Callback: ftp PUT file
//--------------------------------------------------------------------------
static size_t curlRead(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct curl_Transfer *s = (struct curl_Transfer *)userdata;
	CURL *curl = s->curl;
	double curtime = 0;
	curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);
	size_t nread;

	if (curl_sim_fs) {
		if (s->len < 1024) {
			size_t reqsize = size*nmemb;
			nread = 0;
			while ((nread+23) < reqsize) {
				sprintf((char *)(ptr+nread), "%s", "Simulate upload data\r\n");
				nread += 22;
			}
		}
		else nread = 0;
	}
	else {
		nread = fread(ptr, 1, size*nmemb, s->file);
	}

	s->len += nread;
	if ((curl_progress) && ((curtime - s->lastruntime) > curl_progress)) {
		s->lastruntime = curtime;
		printf("* Upload: sent %d\r\n", s->len);
	}

	//printf("**Upload, read %d (%d,%d)\r\n", nread,size,nmemb);
	if (nread <= 0) return 0;

	return nread;
}

/*
//------------------------------------------------------------------
static int closesocket_callback(void *clientp, curl_socket_t item) {
    int ret = lwip_close(item);
    return ret;
}

//------------------------------------------------------------------------------------------------------------
static curl_socket_t opensocket_callback(void *clientp, curlsocktype purpose, struct curl_sockaddr *address) {
    int s = lwip_socket(address->family, address->socktype, address->protocol);
    return s;
}
*/

// Set some common curl options
//----------------------------------------------
static void _set_default_options(CURL *handle) {
	curl_easy_setopt(handle, CURLOPT_VERBOSE, curl_verbose);

	// ** Set SSL Options
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
	//curl_easy_setopt(handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

	// ==== Provide CA Certs from different file than default ====
	//curl_easy_setopt(handle, CURLOPT_CAINFO, "ca-bundle.crt");
	// ===========================================================

	/*
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER , 1L);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST , 1L);
	*/

	/* If the server is redirected, we tell libcurl if we want to follow redirection
	 * There are some problems when redirecting ssl requests, so for now we disable redirection
	 */
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);  // set to 1 to enable redirection
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 3L);

    curl_easy_setopt(handle, CURLOPT_TIMEOUT, (long)curl_timeout);

    curl_easy_setopt(handle, CURLOPT_MAXFILESIZE, (long)curl_maxbytes);
    curl_easy_setopt(handle, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);

    //curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, 1024L);	//bytes/sec
	//curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, 4);		//seconds

    // Open&Close socket callbacks can be set if special handling is needed
    /*
    curl_easy_setopt(handle, CURLOPT_CLOSESOCKETFUNCTION, closesocket_callback);
    curl_easy_setopt(handle, CURLOPT_CLOSESOCKETDATA, NULL);
    curl_easy_setopt(handle, CURLOPT_OPENSOCKETFUNCTION, opensocket_callback);
    curl_easy_setopt(handle, CURLOPT_OPENSOCKETDATA, NULL);
    */
}

//==================================================================================
int Curl_GET(char *url, char *fname, char *hdr, char *body, int hdrlen, int bodylen)
{
	CURL *curl = NULL;
	CURLcode res = 0;
	FILE* file = NULL;
    int err = 0;

    if ((hdr) && (hdrlen < MIN_HDR_BODY_BUF_LEN)) {
        err = -1;
        goto exit;
    }
    if ((body) && (bodylen < MIN_HDR_BODY_BUF_LEN)) {
        err = -2;
        goto exit;
    }

    struct curl_Transfer get_data;
    struct curl_Transfer get_header;

	if (!url) {
        err = -3;
        goto exit;
    }

	if (!curl_initialized) {
		res = curl_global_init(CURL_GLOBAL_DEFAULT);
		if (res) {
            err = -4;
            goto exit;
        }
		curl_initialized = 1;
	}

	// Create a curl curl
	curl = curl_easy_init();
	if (curl == NULL) {
        err = -5;
        goto exit;
	}

    init_curl_Transfer(curl, &get_data, body, bodylen, NULL);
    init_curl_Transfer(curl, &get_header, hdr, hdrlen, NULL);

	if (fname) {
		if (strcmp(fname, "simulate") == 0) {
			get_data.tofile = 1;
			curl_sim_fs = 1;
		}
		else {
			file = fopen(fname, "wb");
			if (file == NULL) {
				err = -6;
				goto exit;
			}
			get_data.file = file;
			get_data.tofile = 1;
			curl_sim_fs = 0;
		}
	}

    curl_easy_setopt(curl, CURLOPT_URL, url);

    _set_default_options(curl);

    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "deflate, gzip");

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &get_header);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &get_data);

	// Perform the request, res will get the return code
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
    	if (curl_verbose) printf("curl_easy_perform failed: %s\r\n", curl_easy_strerror(res));
		if (body) snprintf(body, bodylen, "%s", curl_easy_strerror(res));
        err = -7;
        goto exit;
    }

    if (get_data.tofile) {
    	if (curl_progress) {
			double curtime = 0;
			curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);
			printf("* Download: received %d B; time=%0.1f s; speed=%0.1f KB/sec\r\n", get_data.len, curtime, (float)(((get_data.len*10)/curtime) / 10240.0));
    	}
		if (body) {
			if (strcmp(fname, "simulate") == 0) snprintf(body, bodylen, "SIMULATED save to file; size=%d", get_data.len);
			else snprintf(body, bodylen, "Saved to file %s, size=%d", fname, get_data.len);
		}
    }

exit:
	// Cleanup
    if (file) fclose(file);
    if (curl) curl_easy_cleanup(curl);

    return err;
}


//=======================================================================
int Curl_POST(char *url , char *hdr, char *body, int hdrlen, int bodylen)
{
	CURL *curl = NULL;
	CURLcode res = 0;
    int err = 0;
	struct curl_slist *headerlist=NULL;
	const char hl_buf[] = "Expect:";

    if ((hdr) && (hdrlen < MIN_HDR_BODY_BUF_LEN)) {
        err = -1;
        goto exit;
    }
    if ((body) && (bodylen < MIN_HDR_BODY_BUF_LEN)) {
        err = -2;
        goto exit;
    }

    struct curl_Transfer get_data;
    struct curl_Transfer get_header;

	if (!url) {
        err = -3;
        goto exit;
    }

	if (!curl_initialized) {
		res = curl_global_init(CURL_GLOBAL_DEFAULT);
		if (res) {
            err = -4;
            goto exit;
        }
		curl_initialized = 1;
	}

	// Create a curl curl
	curl = curl_easy_init();
	if (curl == NULL) {
        err = -5;
        goto exit;
	}

    init_curl_Transfer(curl, &get_data, body, bodylen, NULL);
    init_curl_Transfer(curl, &get_header, hdr, hdrlen, NULL);

    // initialize custom header list (stating that Expect: 100-continue is not wanted
	headerlist = curl_slist_append(headerlist, hl_buf);

	// set URL that receives this POST
	curl_easy_setopt(curl, CURLOPT_URL, url);

    _set_default_options(curl);

	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &get_header);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &get_data);

	if (formpost) curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

	// Perform the request, res will get the return code
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
    	if (curl_verbose) printf("curl_easy_perform failed: %s\r\n", curl_easy_strerror(res));
		if (body) snprintf(body, bodylen, "%s", curl_easy_strerror(res));
        err = -7;
        goto exit;
    }

exit:
	// Cleanup
    if (curl) curl_easy_cleanup(curl);
	if (formpost) {
		curl_formfree(formpost);
		formpost = NULL;
	}
	curl_slist_free_all(headerlist);

    return err;
}

#ifdef CONFIG_MICROPY_USE_CURLFTP

//===================================================================================================================
int Curl_FTP(uint8_t upload, char *url, char *user_pass, char *fname, char *hdr, char *body, int hdrlen, int bodylen)
{
#undef DISABLE_SSH_AGENT

	CURL *curl = NULL;
	CURLcode res = 0;
    int err = 0;
	FILE* file = NULL;
	int fsize = 0;

    if ((hdr) && (hdrlen < MIN_HDR_BODY_BUF_LEN)) {
        err = -1;
        goto exit;
    }
    if ((body) && (bodylen < MIN_HDR_BODY_BUF_LEN)) {
        err = -2;
        goto exit;
    }

    struct curl_Transfer get_data;
    struct curl_Transfer get_header;

	if ((!url) || (!user_pass)) {
        err = -3;
        goto exit;
    }

	if (!curl_initialized) {
		res = curl_global_init(CURL_GLOBAL_DEFAULT);
		if (res) {
            err = -4;
            goto exit;
        }
		curl_initialized = 1;
	}

	// Create a curl curl
	curl = curl_easy_init();
	if (curl == NULL) {
        err = -5;
        goto exit;
	}

    init_curl_Transfer(curl, &get_data, body, bodylen, NULL);
    init_curl_Transfer(curl, &get_header, hdr, hdrlen, NULL);

	if (fname) {
		if (strcmp(fname, "simulate") == 0) {
			get_data.tofile = 1;
			curl_sim_fs = 1;
		}
		else {
			if (upload) {
				// Uploading the file
				struct stat sb;
				if ((stat(fname, &sb) == 0) && (sb.st_size > 0)) {
					fsize = sb.st_size;
					file = fopen(fname, "rb");
				}
			}
			else {
				// Downloading to file (LIST or Get file)
				file = fopen(fname, "wb");
			}
			if (file == NULL) {
	            err = -6;
	            goto exit;
			}
			get_data.file = file;
			get_data.tofile = 1;
			get_data.size = fsize;
			curl_sim_fs = 0;
		}
	}

    curl_easy_setopt(curl, CURLOPT_URL, url);

    _set_default_options(curl);

	/// build a list of commands to pass to libcurl
	//headerlist = curl_slist_append(headerlist, "QUIT");
	curl_easy_setopt(curl, CURLOPT_USERPWD, user_pass);

	//curl_easy_setopt(curl, CURLOPT_POSTQUOTE, headerlist);
	curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0L);
	curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &get_header);
    if (upload) {
    	curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR_RETRY);
        // we want to use our own read function
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, curlRead);
        // specify which file to upload
        curl_easy_setopt(curl, CURLOPT_READDATA, &get_data);
        // enable uploading
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

	    if (fsize > 0) curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (long)fsize);
    }
    else {
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &get_data);
    }

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)curl_timeout);

	// Perform the request, res will get the return code
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
    	if (curl_verbose) printf("curl_easy_perform failed: %s\r\n", curl_easy_strerror(res));
		if (body) snprintf(body, bodylen, "%s", curl_easy_strerror(res));
        err = -7;
        goto exit;
    }

    if (get_data.tofile) {
    	if (curl_progress) {
			double curtime = 0;
			curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);
			if (upload) printf("* Upload: sent");
			else printf("* Download: received");
			printf(" %d B; time=%0.1f s; speed=%0.1f KB/sec\r\n", get_data.len, curtime, (float)(((get_data.len*10)/curtime) / 10240.0));
    	}

		if (body) {
	        if (upload) {
				if (strcmp(fname, "simulate") == 0) snprintf(body, bodylen, "SIMULATED upload from file; size=%d", get_data.len);
				else snprintf(body, bodylen, "Uploaded file %s, size=%d", fname, fsize);
	        }
	        else {
				if (strcmp(fname, "simulate") == 0) snprintf(body, bodylen, "SIMULATED download to file; size=%d", get_data.len);
				else snprintf(body, bodylen, "Downloaded to file %s, size=%d", fname, get_data.len);
	        }
		}
    }

exit:
	// Cleanup
    if (file) fclose(file);
    if (curl) curl_easy_cleanup(curl);

    return err;
}

#endif

//-------------------
void Curl_cleanup() {
	if (curl_initialized) {
		curl_global_cleanup();
		curl_initialized = 0;
	}
}

#endif

#ifdef CONFIG_MICROPY_USE_SSH

#include "libssh2_sftp.h"

// ==== LIBSSH2 functions ====

//------------------------------------------------------------
static int waitsocket(int socket_fd, LIBSSH2_SESSION *session)
{
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    FD_ZERO(&fd);

    FD_SET(socket_fd, &fd);

    // now make sure we wait in the correct direction
    dir = libssh2_session_block_directions(session);

    if (dir & LIBSSH2_SESSION_BLOCK_INBOUND) readfd = &fd;

    if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) writefd = &fd;

    rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);

    return rc;
}

//---------------------------------------------------------------------------
static int sock_connect(char *server, char *port, char *messages, int msglen)
{
    int sock=-1;
    int rc;
    char msg[80];
    struct addrinfo *res;
    struct in_addr *addr;
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    // Resolve IP address
    rc = getaddrinfo(server, port, &hints, &res);
    if (rc != 0 || res == NULL) {
    	sprintf(msg, "* DNS lookup failed err=%d res=%p\n", rc, res);
        if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
        	strcat(messages, msg);
        }
    	if (curl_verbose) printf(msg);
        return -1;
    }
    // Print the resolved IP. Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code
    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    sprintf(msg, "* DNS lookup succeeded. IP=%s\n", inet_ntoa(*addr));
    if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
    	strcat(messages, msg);
    }
	if (curl_verbose) printf(msg);

    // Create the socket
    sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
    	sprintf(msg, "* Failed to allocate socket.\n");
        if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
        	strcat(messages, msg);
        }
    	if (curl_verbose) printf(msg);
        freeaddrinfo(res);
        return sock;
    }

    // Connect
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        sprintf(msg, "* Failed to connect!\n");
        if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
        	strcat(messages, msg);
        }
    	if (curl_verbose) printf(msg);
        freeaddrinfo(res);
        close(sock);
        return -1;
    }
    sprintf(msg, "* Connected\n");
    if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
    	strcat(messages, msg);
    }
	if (curl_verbose) printf(msg);
    freeaddrinfo(res);

    return sock;
}

//----------------------------------------------------------------------------------------------------------------------
static LIBSSH2_SESSION *getSSHSession(int sock, char *username, char *password, int auth_pw, char *messages, int msglen)
{
    int rc;
    const char *fingerprint;
    LIBSSH2_SESSION *session;
    char msg[96];

    // Create a session instance
    session = libssh2_session_init();
    if(!session) {
        sprintf(msg, "* Failed to create session!\n");
        if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
        	strcat(messages, msg);
        }
    	if (curl_verbose) printf(msg);
    	return NULL;
    }
    sprintf(msg, "* SSH session created\n");
    if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
    	strcat(messages, msg);
    }
	if (curl_verbose) printf(msg);

    // ... start it up. This will trade welcome banners, exchange keys, and setup crypto, compression, and MAC layers
    rc = libssh2_session_handshake(session, sock);
    if (rc) {
        sprintf(msg, "* Failure establishing SSH session: %d\n", rc);
        if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
        	strcat(messages, msg);
        }
    	if (curl_verbose) printf(msg);
    	return NULL;
    }
    sprintf(msg, "* SSH session established\n");
    if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
    	strcat(messages, msg);
    }
	if (curl_verbose) printf(msg);

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts. Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    sprintf(msg, "* Fingerprint: [");
    for(int i = 0; i < 20; i++) {
        sprintf(msg+15+(i*3), "%02X ", (unsigned char)fingerprint[i]);
    }
    strcat(msg, "]\n");
    if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
    	strcat(messages, msg);
    }
	if (curl_verbose) printf(msg);

    if (auth_pw) {
        // We could authenticate via password
        if (libssh2_userauth_password(session, username, password)) {
            sprintf(msg, "* Authentication by password failed.\n");
            if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
            	strcat(messages, msg);
            }
        	if (curl_verbose) printf(msg);
        	return NULL;
        }
        sprintf(msg,"* Authentication by password succeed.\n");
        if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
        	strcat(messages, msg);
        }
    	if (curl_verbose) printf(msg);
    }
    else {
        // Or by public key
        if (libssh2_userauth_publickey_fromfile(session, username, ".ssh/id_rsa.pub", ".ssh/id_rsa", password)) {
            sprintf(msg, "* Authentication by public key failed\n");
            if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
            	strcat(messages, msg);
            }
        	if (curl_verbose) printf(msg);
        	return NULL;
        }
        sprintf(msg, "* Authentication by public key succeed.\n");
        if ((messages) && ((strlen(messages) + strlen(msg) < msglen))) {
        	strcat(messages, msg);
        }
    	if (curl_verbose) printf(msg);
    }

    return session;
}

//------------------------------------------------------------------------------------------------------------------------------------------
static int sshDownload(LIBSSH2_CHANNEL *channel, libssh2_struct_stat *fileinfo, FILE *fdd, char *hdr, char *body, int hdrlen, int bodylen) {
    libssh2_struct_stat_size got = 0;
    uint32_t tstart = mp_hal_ticks_ms(), tlatest=0;
    int rc, err = 0;
    uint32_t bodypos = 0;
    char msg[80];
    char mem[1024];

    while(got < fileinfo->st_size) {
        int amount = sizeof(mem);

        if ((fileinfo->st_size -got) < amount) {
            amount = (int)(fileinfo->st_size -got);
        }

        rc = libssh2_channel_read(channel, mem, amount);
        if (rc > 0) {
        	if (fdd != NULL) {
				// === Downloading to file ===
        		if (err == 0) {
        			if ((got + amount) < curl_maxbytes) {
						size_t nwrite = fwrite(mem, 1, amount, fdd);
						if (nwrite <= 0) {
							sprintf(msg, "* Download: Error writing to file at %u\n", (uint32_t)got);
							if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
								strcat(hdr, msg);
							}
					    	if (curl_verbose) printf(msg);
							err = -10;
						}
        			}
        			else {
						sprintf(msg, "* Max file size exceeded at %u\n", (uint32_t)got);
						if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
							strcat(hdr, msg);
						}
				    	if (curl_verbose) printf(msg);
        				err = -11;
        			}
        		}
        	}
        	else {
        		// === Download to buffer ===
        		if (err == 0) {
					if ((bodypos + amount) < bodylen) {
						memcpy(body+bodypos, mem, amount);
						bodypos += amount;
						body[bodypos] = 0;
					}
					else {
						sprintf(msg, "* Buffer full at %u\n", (uint32_t)got);
						if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
							strcat(hdr, msg);
						}
				    	if (curl_verbose) printf(msg);
				    	err = -12;
					}
        		}
        	}
        }
        else if( rc < 0) {
			sprintf(msg, "\n* libssh2_channel_read() failed: %d\n", rc);
	        if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
	        	strcat(hdr, msg);
	        }
	    	if (curl_verbose) printf(msg);
	        err = -13;
            break;
        }
        got += rc;
    	if (curl_progress) {
			if (tlatest == 0) {
				printf("\n");
				tlatest = mp_hal_ticks_ms();
			}
			if ((tlatest + (curl_progress*1000)) >= mp_hal_ticks_ms()) {
				tlatest = mp_hal_ticks_ms();
				printf("Download: %u\r", (uint32_t)got);
			}
    	}
    }
    tstart = mp_hal_ticks_ms() - tstart;
	if (curl_progress) {
		printf("                          \r");
	}
    sprintf(msg, "* Received: %u bytes in %0.1f sec (%0.3f KB/s)\n", (uint32_t)got, (float)(tstart / 1000.0), (float)((float)(got)/1024.0/((float)(tstart) / 1000.0)));
    if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
    	strcat(hdr, msg);
    }
	if (curl_verbose) printf(msg);

    return err;
}

//--------------------------------------------------------------------------------
static int sshUpload(LIBSSH2_CHANNEL *channel, FILE *fdd, char *hdr, int hdrlen) {
    uint32_t tstart = mp_hal_ticks_ms(), tlatest=0;
    int rc, err = 0;
    char msg[80];
    char mem[1024];
    size_t nread;
    char *ptr;
    uint32_t sent = 0;

    do {
        nread = fread(mem, 1, sizeof(mem), fdd);
        if (nread <= 0) {
            // end of file
            break;
        }
        ptr = mem;

        do {
            // write the same data over and over, until error or completion
            rc = libssh2_channel_write(channel, ptr, nread);
            if (rc < 0) {
				sprintf(msg, "* Upload: Error sending: %d at %u\n", rc, sent);
				if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
					strcat(hdr, msg);
				}
		    	if (curl_verbose) printf(msg);
                break;
            }
            else {
                // rc indicates how many bytes were written this time
                ptr += rc;
                nread -= rc;
                sent += rc;
            }
        	if (curl_progress) {
    			if (tlatest == 0) {
    				printf("\n");
    				tlatest = mp_hal_ticks_ms();
    			}
    			if ((tlatest + (curl_progress*1000)) >= mp_hal_ticks_ms()) {
    				tlatest = mp_hal_ticks_ms();
    				printf("Upload: %u\r", sent);
    			}
        	}
        } while (nread);

    } while (1);
    tstart = mp_hal_ticks_ms() - tstart;
	if (curl_progress) {
		printf("                          \r");
	}
    sprintf(msg, "* Sent: %u bytes in %0.1f sec (%0.3f KB/s)\n", sent, (float)(tstart / 1000.0), (float)((float)(sent)/1024.0/((float)(tstart) / 1000.0)));
    if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
    	strcat(hdr, msg);
    }
	if (curl_verbose) printf(msg);

    libssh2_channel_send_eof(channel);
    libssh2_channel_wait_eof(channel);
    libssh2_channel_wait_closed(channel);

    return err;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------
int ssh_SCP(uint8_t type, char *server, char *port, char * scppath, char *user, char *pass, char *fname, char *hdr, char *body, int hdrlen, int bodylen)
{

    char msg[80];
    LIBSSH2_SESSION *session;
    libssh2_struct_stat fileinfo;
    FILE *fdd = NULL;
    int fsize = 0;
    hdrlen -=1;
    bodylen -=1;

	if ((fname) && ((type == 0) || (type == 1))){
		if (strcmp(fname, "simulate") != 0) {
			if (type == 1) {
				// Uploading the file
				struct stat sb_local;
				if ((stat(fname, &sb_local) == 0) && (sb_local.st_size > 0)) {
					fsize = sb_local.st_size;
					fdd = fopen(fname, "rb");
				}
			}
			else {
				// Downloading to file (LIST or Get file)
				fdd = fopen(fname, "wb");
			}
			if (fdd == NULL) {
		        sprintf(msg, "* Error opening file\n");
		        if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
		        	strcat(hdr, msg);
		        }
		    	if (curl_verbose) printf(msg);
	            return -5;
			}
		}
	}

	// ** Initialize libssh2
    int rc = libssh2_init (0);
    if (rc != 0) {
        sprintf(msg, "* libssh2 initialization failed (%d)\n", rc);
        if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
        	strcat(hdr, msg);
        }
    	if (curl_verbose) printf(msg);
        if (fdd) fclose(fdd);
        return -1;
    }

    int sock = sock_connect(server, port, hdr, hdrlen);
    if (sock < 0) {
        if (fdd) fclose(fdd);
    	libssh2_exit();
    	return -2;
    }

    // ** Create session
    session = getSSHSession(sock, user, pass, 1, hdr, hdrlen);
    if (session == NULL) {
        if (fdd) fclose(fdd);
    	close(sock);
    	libssh2_exit();
    	return -3;
    }

    LIBSSH2_CHANNEL *channel = NULL;
    // ** Open session
    if (type == 1) {
    	// SCP File upload
        channel = libssh2_scp_send(session, scppath, 0555, (unsigned long)fsize);
        if (!channel) {
            char *errmsg;
            int errlen;
            int err = libssh2_session_last_error(session, &errmsg, &errlen, 0);
            sprintf(msg, "* Unable to open a session: (%d) %s\n", err, errmsg);
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
			if (curl_verbose) printf(msg);
			rc = -4;
            goto shutdown;
        }
		// ** Upload a file via SCP
		rc = sshUpload(channel, fdd, hdr, hdrlen);
    }
    else if (type == 0) {
    	// SCP File download
		channel = libssh2_scp_recv2(session, scppath, &fileinfo);
		if (!channel) {
			sprintf(msg, "* Unable to open a session: %d\n", libssh2_session_last_errno(session));
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
			if (curl_verbose) printf(msg);
			rc = -4;
			goto shutdown;
		}

		if (fileinfo.st_size > curl_maxbytes) {
			sprintf(msg, "* Warning: file size (%u) > max allowed (%u) !\n", (uint32_t)fileinfo.st_size, curl_maxbytes);
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
			if (curl_verbose) printf(msg);
		}
		// ** Download a file via SCP
		rc = sshDownload(channel, &fileinfo, fdd, hdr, body, hdrlen, bodylen);
    }
    else if (type == 5) {
    	// SSH exec, Exec non-blocking on the remove host
    	int bytecount = 0;
        char *exitsignal=(char *)"none";

        while (((channel = libssh2_channel_open_session(session)) == NULL) && (libssh2_session_last_error(session,NULL,NULL,0) == LIBSSH2_ERROR_EAGAIN)) {
            waitsocket(sock, session);
        }
        if (channel == NULL) {
			sprintf(msg, "* Channel Error\n");
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
			if (curl_verbose) printf(msg);
            rc = -1;
            goto endchannel;
        }
		sprintf(msg, "* Exec: '%s'\n", scppath);
		if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
			strcat(hdr, msg);
		}
		if (curl_verbose) printf(msg);

		while( (rc = libssh2_channel_exec(channel, scppath)) == LIBSSH2_ERROR_EAGAIN ) {
            waitsocket(sock, session);
        }
        if ( rc != 0 ) {
			sprintf(msg, "* Channel Exec Error %d\n", rc);
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
			if (curl_verbose) printf(msg);
            goto endchannel;
        }
        char buffer[1024];
        int body_pos = 0;
        for( ;; ) {
            // loop until we block
            int rc;
            do {
                rc = libssh2_channel_read( channel, buffer, sizeof(buffer) );
                if (rc > 0) {
                    if ((body_pos+rc) < bodylen) {
                    	memcpy(body+body_pos, buffer, rc);
                    	body_pos += rc;
                    }
                    bytecount += rc;
                }
            }
            while( rc > 0 );

            // this is due to blocking that would occur otherwise so we loop on this condition
            if ( rc == LIBSSH2_ERROR_EAGAIN ) waitsocket(sock, session);
            else break;
        }
        int exitcode = 127;
        while ( (rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN ) waitsocket(sock, session);

        if ( rc == 0 ) {
            exitcode = libssh2_channel_get_exit_status( channel );
            libssh2_channel_get_exit_signal(channel, &exitsignal, NULL, NULL, NULL, NULL, NULL);
        }

        if (exitsignal) {
			sprintf(msg, "* Got signal '%s'\n", exitsignal);
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
			if (curl_verbose) printf(msg);
			rc = -1;
        }
        else {
			sprintf(msg, "* Exit: %d bytecount: %d\n", exitcode, bytecount);
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
			if (curl_verbose) printf(msg);
			rc = exitcode;
        }
    }
    else if (type == 4) {
    	// SFTP mkdir
        LIBSSH2_SFTP *sftp_session;

        sftp_session = libssh2_sftp_init(session);
        if (!sftp_session) {
            sprintf(msg, "Unable to init SFTP session\n");
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
            goto shutdown;
        }
        // Since we have not set non-blocking, tell libssh2 we are blocking
        libssh2_session_set_blocking(session, 1);
        // Make a directory via SFTP
        rc = libssh2_sftp_mkdir(sftp_session, scppath,
                                LIBSSH2_SFTP_S_IRWXU|
                                LIBSSH2_SFTP_S_IRGRP|LIBSSH2_SFTP_S_IXGRP|
                                LIBSSH2_SFTP_S_IROTH|LIBSSH2_SFTP_S_IXOTH);

        if (rc) {
            sprintf(msg, "SFTP mkdir failed\n");
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
        }
        libssh2_sftp_shutdown(sftp_session);
    }
    else if ((type == 2) || (type == 3)) {
    	// SFTP List
        LIBSSH2_SFTP *sftp_session;
        LIBSSH2_SFTP_HANDLE *sftp_handle;

        sftp_session = libssh2_sftp_init(session);
        if (!sftp_session) {
            sprintf(msg, "Unable to init SFTP session\n");
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
            goto shutdown;
        }
        // Since we have not set non-blocking, tell libssh2 we are blocking
        libssh2_session_set_blocking(session, 1);
        // Request a dir listing via SFTP
        sftp_handle = libssh2_sftp_opendir(sftp_session, scppath);
        if (!sftp_handle) {
            sprintf(msg, "Unable to open dir with SFTP\n");
			if ((hdr) && ((strlen(hdr) + strlen(msg) < hdrlen))) {
				strcat(hdr, msg);
			}
            goto shutdown;
        }
        int body_pos = 0;
        do {
            char mem[128];
            char longentry[256];
            LIBSSH2_SFTP_ATTRIBUTES attrs;
            attrs.flags = LIBSSH2_SFTP_ATTR_SIZE | LIBSSH2_SFTP_ATTR_ACMODTIME;

            /* loop until we fail */
            rc = libssh2_sftp_readdir_ex(sftp_handle, mem, sizeof(mem), longentry, sizeof(longentry), &attrs);
            if (rc > 0) {
                // rc is the length of the file name in the mem buffer

                if ((longentry[0] != '\0') && (type == 3)) {
                    if ((body_pos+strlen(longentry)) < bodylen) {
                    	sprintf(body+body_pos, "%s\n", longentry);
                    	body_pos += strlen(longentry) + 1;
                    }
                } else {
                    if (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) {
                    	uint32_t prm = attrs.permissions >> 12;
                    	if (prm == 4) sprintf(msg, "D\t");
                    	else if (prm == 8) sprintf(msg, "F\t");
                    	else if (prm == 10) sprintf(msg, "L\t");
                    	else sprintf(msg, "?\t");
                    }
                    else sprintf(msg, "?\t");

                    if (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) sprintf(msg+strlen(msg), "%llu\t" , (uint64_t)attrs.filesize);
                    else (sprintf(msg, "0\t"));

                    if ((body_pos+strlen(msg)+strlen(mem)+1) < bodylen) {
                    	sprintf(body+body_pos, "%s%s\n", msg, mem);
                    	body_pos += strlen(msg) + strlen(mem) + 1;
                    }
                }
            }
            else
                break;

        } while (1);

        libssh2_sftp_closedir(sftp_handle);
        libssh2_sftp_shutdown(sftp_session);
    }

endchannel:
    if (channel) {
    	libssh2_channel_free(channel);
        channel = NULL;
    }

shutdown:
	if (fdd) fclose(fdd);
	libssh2_session_disconnect(session, "Normal Shutdown.");
	libssh2_session_free(session);
	close(sock);
	libssh2_exit();
	if (curl_verbose) printf("All done\n");

	return rc;
}

#endif

#endif
