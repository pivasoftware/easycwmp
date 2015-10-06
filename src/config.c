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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <microxml.h> 
#include <time.h>
#include "cwmp.h"
#include "backup.h" 
#include "config.h"
#include "log.h"

static bool first_run = true;
static struct uci_context *uci_ctx;
static struct uci_package *uci_easycwmp;

struct core_config *config;


static void config_free_local(void) {
	if (config->local) {
		FREE(config->local->ip);
		FREE(config->local->interface);
		FREE(config->local->port);
		FREE(config->local->username);
		FREE(config->local->password);
		FREE(config->local->ubus_socket);
	}
}

static int config_init_local(void)
{
	struct uci_section *s;
	struct uci_element *e1;

	uci_foreach_element(&uci_easycwmp->sections, e1) {
		s = uci_to_section(e1);
		if (strcmp(s->type, "local") == 0) {
			config_free_local();

			config->local->logging_level = DEFAULT_LOGGING_LEVEL;
			uci_foreach_element(&s->options, e1) {
				if (!strcmp((uci_to_option(e1))->e.name, "interface")) {
					config->local->interface = strdup(uci_to_option(e1)->v.string);
					DD("easycwmp.@local[0].interface=%s\n", config->local->interface);
					continue;
				}

				if (!strcmp((uci_to_option(e1))->e.name, "port")) {
					if (!atoi((uci_to_option(e1))->v.string)) {
						D("in section local port has invalid value...\n");
						return -1;
					}
					config->local->port = strdup(uci_to_option(e1)->v.string);
					DD("easycwmp.@local[0].port=%s\n", config->local->port);
					continue;
				}

				if (!strcmp((uci_to_option(e1))->e.name, "username")) {
					config->local->username = strdup(uci_to_option(e1)->v.string);
					DD("easycwmp.@local[0].username=%s\n", config->local->username);
					continue;
				}

				if (!strcmp((uci_to_option(e1))->e.name, "password")) {
					config->local->password = strdup(uci_to_option(e1)->v.string);
					DD("easycwmp.@local[0].password=%s\n", config->local->password);
					continue;
				}

				if (!strcmp((uci_to_option(e1))->e.name, "ubus_socket")) {
					config->local->ubus_socket = strdup(uci_to_option(e1)->v.string);
					DD("easycwmp.@local[0].ubus_socket=%s\n", config->local->ubus_socket);
					continue;
				}
				
        if (!strcmp((uci_to_option(e1))->e.name, "logging_level")) {
					char *c;
					int log_level = atoi((uci_to_option(e1))->v.string);					 
					asprintf(&c, "%d", log_level);
					if (!strcmp(c, uci_to_option(e1)->v.string)) 
						config->local->logging_level = log_level;						
					free(c);
					DD("easycwmp.@local[0].logging_level=%d\n", config->local->logging_level);
					continue;
				}
			}

			if (!config->local->interface) {
				D("in local you must define interface\n");
				return -1;
			}

			if (!config->local->port) {
				D("in local you must define port\n");
				return -1;
			}

			if (!config->local->ubus_socket) {
				D("in local you must define ubus_socket\n");
				return -1;
			}

			return 0;
		}
	}
	D("uci section local not found...\n");
	return -1;
}

static void config_free_acs(void) {
	if (config->acs) {
		FREE(config->acs->url);
		FREE(config->acs->username);
		FREE(config->acs->password);
		FREE(config->acs->ssl_cert);
		FREE(config->acs->ssl_cacert);
	}
}

static int config_init_acs(void)
{
	struct uci_section *s;
	struct uci_element *e;
	struct tm tm;

	uci_foreach_element(&uci_easycwmp->sections, e) {
		s = uci_to_section(e);
		if (strcmp(s->type, "acs") == 0) {
			config_free_acs();
			config->acs->periodic_time = -1;

			uci_foreach_element(&s->options, e) {
				if (!strcmp((uci_to_option(e))->e.name, "url")) {
					bool valid = false;

					if (!(strncmp((uci_to_option(e))->v.string, "http:", 5)))
						valid = true;

					if (!(strncmp((uci_to_option(e))->v.string, "https:", 6)))
						valid = true;

					if (!valid) {
						D("in section acs scheme must be either http or https...\n");
						return -1;
					}

					config->acs->url = strdup(uci_to_option(e)->v.string);
					DD("easycwmp.@acs[0].url=%s\n", config->acs->url);
					continue;
				}

				if (!strcmp((uci_to_option(e))->e.name, "username")) {
					config->acs->username = strdup(uci_to_option(e)->v.string);
					DD("easycwmp.@acs[0].username=%s\n", config->acs->username);
					continue;
				}

				if (!strcmp((uci_to_option(e))->e.name, "password")) {
					config->acs->password = strdup(uci_to_option(e)->v.string);
					DD("easycwmp.@acs[0].password=%s\n", config->acs->password);
					continue;
				}

				if (!strcmp((uci_to_option(e))->e.name, "periodic_enable")) {
					config->acs->periodic_enable = (atoi((uci_to_option(e))->v.string) == 1) ? true : false;
					DD("easycwmp.@acs[0].periodic_enable=%d\n", config->acs->periodic_enable);
					continue;
				}

				if (!strcmp((uci_to_option(e))->e.name, "periodic_interval")) {
					config->acs->periodic_interval = atoi((uci_to_option(e))->v.string);
					DD("easycwmp.@acs[0].periodic_interval=%d\n", config->acs->periodic_interval);
					continue;
				}

				if (!strcmp((uci_to_option(e))->e.name, "periodic_time")) {
					strptime(uci_to_option(e)->v.string,"%FT%T", &tm);
					config->acs->periodic_time = mktime(&tm);
					DD("easycwmp.@acs[0].periodic_time=%s\n", uci_to_option(e)->v.string);
					continue;
				}

				if (!strcmp((uci_to_option(e))->e.name, "http100continue_disable")) {
					config->acs->http100continue_disable = (atoi(uci_to_option(e)->v.string)) ? true : false;
					DD("easycwmp.@acs[0].http100continue_disable=%d\n", config->acs->http100continue_disable);
					continue;
				}

				if (!strcmp((uci_to_option(e))->e.name, "ssl_cert")) {
					config->acs->ssl_cert = strdup(uci_to_option(e)->v.string);
					DD("easycwmp.@acs[0].ssl_cert=%s\n", config->acs->ssl_cert);
					continue;
				}
				if (!strcmp((uci_to_option(e))->e.name, "ssl_cacert")) {
					config->acs->ssl_cacert = strdup(uci_to_option(e)->v.string);
					DD("easycwmp.@acs[0].ssl_cacert=%s\n", config->acs->ssl_cacert);
					continue;
				}

				if (!strcmp((uci_to_option(e))->e.name, "ssl_verify")) {
					if (!strcmp((uci_to_option(e))->v.string, "enabled")) {
						config->acs->ssl_verify = true;
					} else {
						config->acs->ssl_verify = false;
					}
					DD("easycwmp.@acs[0].ssl_verify=%d\n", config->acs->ssl_verify);
					continue;
				}
			}

			if (!config->acs->url) {
				D("acs url must be defined in the config\n");
				return -1;
			}

			return 0;
		}
	}
	D("uci section acs not found...\n");
	return -1;
}

static struct uci_package *
config_init_package(const char *c)
{
	if (first_run) {
		config = calloc(1, sizeof(struct core_config));
		if (!config) goto error;

		config->acs = calloc(1, sizeof(struct acs));
		if (!config->acs) goto error;

		config->local = calloc(1, sizeof(struct local));
		if (!config->local) goto error;
	}
	if (!uci_ctx) {
		uci_ctx = uci_alloc_context();
		if (!uci_ctx) goto error;
	} else {
		if (uci_easycwmp) {
			uci_unload(uci_ctx, uci_easycwmp);
			uci_easycwmp = NULL;
		}
	}
	if (uci_load(uci_ctx, c, &uci_easycwmp)) {
		uci_free_context(uci_ctx);
		uci_ctx = NULL;
		return NULL;
	}
	return uci_easycwmp;

error:
	config_exit();
	return NULL;
}

static inline void config_free_ctx(void)
{
	if (uci_ctx) {
		if (uci_easycwmp) {
			uci_unload(uci_ctx, uci_easycwmp);
			uci_easycwmp = NULL;
		}
		uci_free_context(uci_ctx);
		uci_ctx = NULL;
	}
}

void config_exit(void)
{
	if (config) {
		config_free_acs();
		FREE(config->acs);
		config_free_local();
		FREE(config->local);
		FREE(config);
	}
	config_free_ctx();
}

void config_load(void)
{

	uci_easycwmp = config_init_package("easycwmp");

	if (!uci_easycwmp) goto error;
	if (config_init_local()) goto error;
	if (config_init_acs()) goto error;

	backup_check_acs_url();
	cwmp_periodic_inform_init();

	first_run = false;
	config_free_ctx();

	cwmp_update_value_change();
	return;

error:
	log_message(NAME, L_CRIT, "configuration (re)loading failed, exit daemon\n");
	D("configuration (re)loading failed\n"); 
	exit(EXIT_FAILURE);
}

