/* Agent restarting function
 * Copyright (C) 2017 Wazuh Inc.
 * Aug 23, 2017.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "shared.h"
#include "headers/defs.h"
#include "os_execd/execd.h"
#include "os_net/os_net.h"
#include "config/syscheck-config.h"
#include "config/rootcheck-config.h"
#include "config/localfile-config.h"
#include "config/client-config.h"
#include "wazuh_modules/wmodules.h"
#include "agentd.h"

static const char AG_IN_RCON[] = "wazuh: Invalid remote configuration";

void * restartAgent() {

	char req[] = "restart";
	ssize_t length;

	length = strlen(req);

	#ifndef WIN32

	int sock = -1;
	char sockname[PATH_MAX + 1];

	if (isChroot()) {
		strcpy(sockname, COM_LOCAL_SOCK);
	} else {
		strcpy(sockname, DEFAULTDIR COM_LOCAL_SOCK);
	}

	if (sock = OS_ConnectUnixDomain(sockname, SOCK_STREAM, OS_MAXSTR), sock < 0) {
		merror("At restartAgent(): Could not connect to socket '%s': %s (%d).", sockname, strerror(errno), errno);
	} else {
		if (send(sock, req, length, 0) != length) {
			merror("send(): %s", strerror(errno));
		}

		close(sock);
	}

	#else

	char output[OS_MAXSTR + 1];
	length = wcom_dispatch(req, length, output);

	#endif

	return NULL;
}

int verifyRemoteConf(){
	const char *configPath;
 	char msg_output[OS_MAXSTR];

	if (isChroot()) {
		configPath = AGENTCONFIGINT;

	} else {
		configPath = AGENTCONFIG;
	}

	if (Test_Syscheck(configPath) < 0) {
		snprintf(msg_output, OS_MAXSTR, "%c:%s:%s: '%s'. ",  LOCALFILE_MQ, "ossec-agent", AG_IN_RCON, "syscheck");
		goto fail;
	} else if (Test_Rootcheck(configPath) < 0) {
		snprintf(msg_output, OS_MAXSTR, "%c:%s:%s: '%s'. ",  LOCALFILE_MQ, "ossec-agent", AG_IN_RCON, "rootcheck");
		goto fail;
    } else if (Test_Localfile(configPath) < 0) {
		snprintf(msg_output, OS_MAXSTR, "%c:%s:%s: '%s'. ",  LOCALFILE_MQ, "ossec-agent", AG_IN_RCON, "localfile");
		goto fail;
    } else if (Test_Client(configPath) < 0) {
		snprintf(msg_output, OS_MAXSTR, "%c:%s:%s: '%s'. ",  LOCALFILE_MQ, "ossec-agent", AG_IN_RCON, "client");
		goto fail;
	} else if (Test_ClientBuffer(configPath) < 0) {
		snprintf(msg_output, OS_MAXSTR, "%c:%s:%s: '%s'. ",  LOCALFILE_MQ, "ossec-agent", AG_IN_RCON, "client_buffer");
		goto fail;
    } else if (Test_WModule(configPath) < 0) {
		snprintf(msg_output, OS_MAXSTR, "%c:%s:%s: '%s'. ",  LOCALFILE_MQ, "ossec-agent", AG_IN_RCON, "wodle");
		goto fail;
    } else if (Test_Labels(configPath) < 0) {
		snprintf(msg_output, OS_MAXSTR, "%c:%s:%s: '%s'. ",  LOCALFILE_MQ, "ossec-agent", AG_IN_RCON, "labels");
		goto fail;
    }

	return 0;

	fail:
		mdebug2("Invalid remote configuration received");
		send_msg(msg_output, -1);
		return OS_INVALID;
};
