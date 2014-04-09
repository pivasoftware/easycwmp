/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Copyright (C) 2012-2014 PIVA SOFTWARE (www.pivasoftware.com)
 *		Author: Mohamed Kallel <mohamed.kallel@pivasoftware.com>
 *		Author: Anis Ellouze <anis.ellouze@pivasoftware.com>
 *	Copyright (C) 2012 Luka Perkov <freecwmp@lukaperkov.net>
 */

#include <unistd.h>
#include <libubus.h>

#include "ubus.h"

#include "config.h"
#include "cwmp.h"
#include "easycwmp.h"
#include "external.h"
#include "log.h"

static struct ubus_context *ctx = NULL;
static struct ubus_object main_object;
static struct blob_buf b;

static struct uloop_timeout ubus_timer;

static void ubus_easycwmpd_stop_callback(struct uloop_timeout *timeout)
{
	ubus_remove_object(ctx, &main_object);
	uloop_end();
}

enum ubus_notify {
	NOTIFY_PARAM,
	NOTIFY_VALUE,
	NOTIFY_TYPE,
	__NOTIFY_MAX
};

static const struct blobmsg_policy notify_policy[] = {
	[NOTIFY_PARAM] = { .name = "parameter", .type = BLOBMSG_TYPE_STRING },
	[NOTIFY_VALUE] = { .name = "value", .type = BLOBMSG_TYPE_STRING },
	[NOTIFY_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_STRING },
};

static int
easycwmpd_handle_notify(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	struct blob_attr *tb[__NOTIFY_MAX];

	blobmsg_parse(notify_policy, ARRAY_SIZE(notify_policy), tb,
			  blob_data(msg), blob_len(msg));

	if (!tb[NOTIFY_PARAM])
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (!tb[NOTIFY_VALUE])
		return UBUS_STATUS_INVALID_ARGUMENT;

	log_message(NAME, L_NOTICE, "triggered ubus notification parameter %s\n",
			blobmsg_data(tb[NOTIFY_PARAM]));
	cwmp_add_notification(blobmsg_data(tb[NOTIFY_PARAM]),
			blobmsg_data(tb[NOTIFY_VALUE]),
			tb[NOTIFY_TYPE] ? blobmsg_data(tb[NOTIFY_TYPE]) : NULL);

	return 0;
}

enum ubus_inform {
	INFORM_EVENT,
	__INFORM_MAX
};

static const struct blobmsg_policy inform_policy[] = {
	[INFORM_EVENT] = { .name = "event", .type = BLOBMSG_TYPE_STRING },
};

static int
easycwmpd_handle_inform(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	int tmp;
	struct blob_attr *tb[__INFORM_MAX];

	blobmsg_parse(inform_policy, ARRAY_SIZE(inform_policy), tb,
			  blob_data(msg), blob_len(msg));

	if (!tb[INFORM_EVENT])
		return UBUS_STATUS_INVALID_ARGUMENT;

	log_message(NAME, L_NOTICE, "triggered ubus inform %s\n",
			blobmsg_data(tb[INFORM_EVENT]));
	tmp = cwmp_get_int_event_code(blobmsg_data(tb[INFORM_EVENT]));
	cwmp_connection_request(tmp);

	return 0;
}

enum ubus_command {
	COMMAND_NAME,
	__COMMAND_MAX
};

static const struct blobmsg_policy command_policy[] = {
	[COMMAND_NAME] = { .name = "name", .type = BLOBMSG_TYPE_STRING },
};

static int
easycwmpd_handle_command(struct ubus_context *ctx, struct ubus_object *obj,
			 struct ubus_request_data *req, const char *method,
			 struct blob_attr *msg)
{
	struct blob_attr *tb[__COMMAND_MAX];

	blobmsg_parse(command_policy, ARRAY_SIZE(command_policy), tb,
			  blob_data(msg), blob_len(msg));

	if (!tb[COMMAND_NAME])
		return UBUS_STATUS_INVALID_ARGUMENT;

	blob_buf_init(&b, 0);

	char *cmd = blobmsg_data(tb[COMMAND_NAME]);
	char *info;

	if (!strcmp("reload", cmd)) {
		log_message(NAME, L_NOTICE, "triggered ubus reload\n");
		easycwmp_reload();
		blobmsg_add_u32(&b, "status", 0);
		if (asprintf(&info, "easycwmpd reloaded") == -1)
			return -1;
	} else if (!strcmp("stop", cmd)) {
		log_message(NAME, L_NOTICE, "triggered ubus stop\n");
		ubus_timer.cb = ubus_easycwmpd_stop_callback;
		uloop_timeout_set(&ubus_timer, 1000);
		blobmsg_add_u32(&b, "status", 0);
		if (asprintf(&info, "easycwmpd stopped") == -1)
			return -1;
	} else {
		blobmsg_add_u32(&b, "status", -1);
		if (asprintf(&info, "%s command is not supported", cmd) == -1)
			return -1;
	}

	blobmsg_add_string(&b, "info", info);
	free(info);

	ubus_send_reply(ctx, req, b.head);

	blob_buf_free(&b);

	return 0;
}

static const struct ubus_method easycwmp_methods[] = {
	UBUS_METHOD("notify", easycwmpd_handle_notify, notify_policy),
	UBUS_METHOD("inform", easycwmpd_handle_inform, inform_policy),
	UBUS_METHOD("command", easycwmpd_handle_command, command_policy),
};

static struct ubus_object_type main_object_type =
	UBUS_OBJECT_TYPE("easycwmpd", easycwmp_methods);

static struct ubus_object main_object = {
	.name = "tr069",
	.type = &main_object_type,
	.methods = easycwmp_methods,
	.n_methods = ARRAY_SIZE(easycwmp_methods),
};

int
ubus_init(void)
{
	ctx = ubus_connect(config->local->ubus_socket);
	if (!ctx) return -1;

	ubus_add_uloop(ctx);

	if (ubus_add_object(ctx, &main_object)) return -1;

	return 0;
}

void
ubus_exit(void)
{
	if (ctx) ubus_free(ctx);
}
