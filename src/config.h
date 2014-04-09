/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2012-2014 PIVA SOFTWARE (www.pivasoftware.com)
 *		Author: Mohamed Kallel <mohamed.kallel@pivasoftware.com>
 *		Author: Anis Ellouze <anis.ellouze@pivasoftware.com>
 *	Copyright (C) 2011 Luka Perkov <freecwmp@lukaperkov.net>
 */

#ifndef _EASYCWMP_CONFIG_H__
#define _EASYCWMP_CONFIG_H__

#include <uci.h>

#include "easycwmp.h"

void config_exit(void);
void config_load(void);
int config_remove_event(char *event);
int config_check_acs_url(void);

struct acs {
	char *scheme;
	char *username;
	char *password;
	char *hostname;
	char *port;
	char *path;
	bool periodic_enable;
	int  periodic_interval;
	char *ssl_cert;
	char *ssl_cacert;
	bool ssl_verify;
};

struct local {
	char *ip;
	char *interface;
	char *port;
	char *username;
	char *password;
	char *ubus_socket;
};

struct core_config {
	struct acs *acs;
	struct local *local;
};

extern struct core_config *config;

#endif

